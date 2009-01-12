#include "PreLoadFs.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>
#include <string.h>
#include <iostream>
#include <boost/scoped_array.hpp>

extern bool g_DebugMode;

PreLoadFs::PreLoadFs(std::string tmpPath, size_t tmpSize, std::string fileToMount) :
	m_name(fileToMount),
	m_refs(0),
	m_offset(0),
	m_offsetBuf(0),
	m_buffer(tmpPath, tmpSize),
	m_exception(false),
	m_seeked(false),
	m_wasAlmostFull(false)
{
}

PreLoadFs::~PreLoadFs()
{
}

void *PreLoadFs::init()
{
	/** We are read only buffering 'filesystem'. So we open
	 *  a file as read only.
	**/
	m_fd = ::open(m_name.string().c_str(), O_RDONLY);

	if (m_fd != -1)
	{
		int r = pthread_create(&m_thread, NULL, &PreLoadFs::runT, this);
		if (r != 0)
		{
			/** Failed to create thread. Clean up used resources.
			**/
			::close(m_fd);
			m_fd = -1;
		}
	}
	return NULL;
}

void PreLoadFs::destroy(void *arg)
{
	if (m_fd != -1)
	{
		/** Cancel thread and wait to its termination.
		**/
		pthread_cancel(m_thread);
		pthread_join(m_thread, NULL);

		close(m_fd);
		m_fd = -1;
	}

	assert(m_refs == 0);
	assert(m_fd == -1);
}

int PreLoadFs::getattr(const char *name, struct stat *st)
{
	int r = 0;

	memset(st, 0, sizeof(struct stat));

	if (g_DebugMode)
		std::cout << __PRETTY_FUNCTION__ << ", name: " << name << std::endl;

	if (name[0] == '/')
		++name;

	if (name[0] == '\0')
	{
		/** We depend on chdir beeing called in main().
		**/
		r = ::stat(".", st);
	}
	else
	{
		/** File name of the mounted file.
		**/
		boost::filesystem::path tmp(m_name.leaf());

		if (tmp.string().compare(name) == 0)
		{
			/** Get stats of mounted file
			**/
			r = ::stat(m_name.string().c_str(), st);
		}
		else
			/** Only mounted file is visible in mount point.
			**/
			r = -ENOENT;
	}

	return r;
}

int PreLoadFs::readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	struct stat st;

	memset(&st, 0, sizeof(st));
	st.st_ino = 1;
	st.st_mode = S_IFDIR;
	filler(buf, ".", &st, 0);

	memset(&st, 0, sizeof(st));
	st.st_ino = 2;
	st.st_mode = S_IFDIR;
	filler(buf, "..", &st, 0);

	memset(&st, 0, sizeof(st));
	st.st_ino = 3;
	st.st_mode = S_IFREG;
	boost::filesystem::path tmp(m_name.leaf());
	filler(buf, tmp.string().c_str(), &st, 0);

	return 0;
}

int PreLoadFs::open(const char *name, struct fuse_file_info *fi)
{
	if (g_DebugMode)
		std::cout << __PRETTY_FUNCTION__ << ": m_fd: " << m_fd << std::endl;

	/** File name of the mounted file.
	**/
	boost::filesystem::path tmp(m_name.leaf());

	/** Allow to open only our mounted file.
	**/
	if (tmp.string().compare(&name[1]) != 0)
		return -ENOENT;

	/** Allow to open only if we sucessfuly opened the file.
	 **/
	if (m_fd == -1)
		return -ENOENT;

	/** Allow to open only in read only mode.
	**/
	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;

	++m_refs;

	return 0;
}

int PreLoadFs::release(const char *name, struct fuse_file_info * /*fi*/)
{
	if (--m_refs == 0)
	{
		/** Set flags to default state.
		**/
		m_exception = false;
		m_seeked = false;
	}
	return 0;
}

int PreLoadFs::seek(off_t offset)
{
	assert(m_offset != offset);

	if (g_DebugMode)
		std::cout << __PRETTY_FUNCTION__ << " " << offset << std::endl;

	pthread_mutex_lock(&m_mutex);

	off_t r;

	if ((m_offset < offset) && (m_offset + m_buffer.full() > offset))
	{
		if (g_DebugMode)
			std::cout << __PRETTY_FUNCTION__ << " fast m_offset:" << m_offset << ", offset:" << offset << ", ->" << m_offset + m_buffer.full() << std::endl;

		/** There are data in the buffer covering required offset.
		**/
		assert(offset - m_offset > 0);
		m_buffer.advance(offset - m_offset);
		r = offset;
	}
	else
	{
		/** Clear the circular buffer.
		**/
		m_buffer.clear();

		/** Seek to required position in opened file.
		**/
		r = ::lseek(m_fd, offset, SEEK_SET);
		if (r == (off_t) -1)
		{
			/** lseek() failed, unlock mutex and return
			 *  error return value.
			**/
			pthread_mutex_unlock(&m_mutex);
			return -errno;
		}

		/** Set flag to let know the thread that we seeked to
		 *  a new offset (It has to eventually discard data
		 *  that has been read and not stored to circular buffer
		 *  yet.
		**/
		m_seeked = true;

		/** Clear exception flag. It might be set when end of
		 *  file detected, so seek should clear this exception.
		**/
		m_exception = false;

		m_offsetBuf = r;
	}

	/** Set offset to new value.
	**/
	m_offset = r;

	m_wasAlmostFull = false;

	/** Let know the thread that it can read new data.
	**/
	pthread_cond_signal(&m_wakeupReadNewData);
	pthread_mutex_unlock(&m_mutex);
	return 0;
}

int PreLoadFs::read(const char *name, char *buf, size_t len, off_t offset, struct fuse_file_info * /*fi*/)
{
	char *orig_buf = buf;

	if (g_DebugMode)
		std::cout << __PRETTY_FUNCTION__<< std::hex <<"offset: " << offset << ", len: " << len << ", m_offset: " << m_offset << std::endl;

	/** File name of the mounted file.
	**/
	boost::filesystem::path tmp(m_name.leaf());

	/** Assert that user can read only from our mounted file.
	**/
	assert(tmp.string().compare(&name[1]) == 0);

	/** Perform seek if user wants to read from offset
	 *  different than we have currently.
	**/
	if (m_offset != offset)
	{
		int r = seek(offset);
		if (r != 0)
			return r;
	}

	while (len > 0)
	{
		pthread_mutex_lock(&m_mutex);

		if (g_DebugMode)
			m_buffer.stats();

		while (!m_buffer.isAlmostFull() && (m_wasAlmostFull == false) && (m_exception == false))
		{
			if (g_DebugMode)
				std::cout << __PRETTY_FUNCTION__ << ", isAlmostFull: " << m_buffer.isAlmostFull() <<
				                                    ", wasAlmostFull: " << m_wasAlmostFull <<
				                                    ", exception: " << m_exception << std::endl;

			/** Wait for a new data if buffer is not almost full or exception
			 *  is detected (when exception is detected there will be no more
			 *  data so we have to read what's available because there will
			 *  not be any new data.
			**/
			pthread_cond_wait(&m_wakeupNewData, &m_mutex);
		}

		if (m_buffer.isAlmostFull())
			m_wasAlmostFull = true;
 
		/** Read data from buffer.
		**/
		int r = m_buffer.get(buf, len);

		if (g_DebugMode)
			std::cout << __PRETTY_FUNCTION__ << ", get returned: " << r << " (" << strerror(errno) << ")" << std::endl;

		if (r == -1)
		{
			pthread_mutex_unlock(&m_mutex);
			break;
		}

		buf += r;
		len -= r;

		/** Advance offset.
		**/
		m_offset += r;

		/** Detect exception only if there are no data in the buffer.
		**/
		if (r == 0)
		{
			m_wasAlmostFull = false;

			if (m_exception == true)
			{
				if (m_error != 0)
				{
					//Error detected when read...

					pthread_mutex_unlock(&m_mutex);

					assert(m_error > 0);
					return -m_error;
				}

				// Just end of file...

				pthread_mutex_unlock(&m_mutex);
				break;
			}
		}
		/** Let know the thread that it can read new data.
		**/
		pthread_cond_signal(&m_wakeupReadNewData);

		pthread_mutex_unlock(&m_mutex);
	}

	/** Compute total bytes read.
	**/
	int t = buf - orig_buf;

	/** Return total bytes read.
	**/
	return t;
}

void *PreLoadFs::runT(void *arg)
{
	reinterpret_cast<PreLoadFs*>(arg)->run();

	/** Function run() is expected to never return...
	**/
	return NULL;
}

void PreLoadFs::run()
{
	int buf_size = 4096;

	/** FIX: This is memory leak. We ignore it now because
	 *       if this thread is canceled the whole application
	 *       exits.
	 **/
	char* buf = new char[buf_size];

	while (true)
	{
		pthread_mutex_lock(&m_mutex);

		if (g_DebugMode)
			m_buffer.stats();

		/** Wait until buffer is not full or exception is resolved.
		**/
		while (m_buffer.isFull() || (m_exception == true))
		{
			if (g_DebugMode)
				std::cout << __PRETTY_FUNCTION__ << ", isFull: " << m_buffer.isFull() <<
			                                            ", exception: " << m_exception << std::endl;

			pthread_cond_wait(&m_wakeupReadNewData, &m_mutex);
		}

		if (m_seeked == true)
			m_seeked = false;

		off_t offset = m_offsetBuf;

		pthread_mutex_unlock(&m_mutex);

		/** This read() may take a long time, thus we don't hold
		 *  the mutex.
		 *  TODO: This should be rewritten to use select to be able to
		 *  cancel download imediately.
		**/
		if (g_DebugMode)
			std::cout << __PRETTY_FUNCTION__ << "..reading: " << buf_size << std::endl;

		int r = ::pread(m_fd, buf, buf_size, offset);

		if (g_DebugMode)
			std::cout << __PRETTY_FUNCTION__ << "..read: " << r << std::endl;

		pthread_mutex_lock(&m_mutex);

		if (m_seeked == true)
		{
			if (g_DebugMode)
				std::cout << __PRETTY_FUNCTION__ << "..seeked...continue" << std::endl;

			m_seeked = false;

			pthread_mutex_unlock(&m_mutex);

			/** User seeked before we stored data in the buffer,
			 *  by continue we discard them and start again.
			**/
			continue;
		}

		if (r == -1)
		{
			/** Error during read. Set exception flag and error
			 *  type code.
			**/
			m_exception = true;
			m_error = errno;
		}
		else if (r == 0)
		{
			/** End of file detected. Set exception flag and
			 *  error type code set to zero.
			**/
			m_exception = true;
			m_error = 0;
		}
		else
		{
			m_offsetBuf += r;

			if (g_DebugMode)
				std::cout << __PRETTY_FUNCTION__ << "..pushing" << std::endl;

			/** Store data to the buffer.
			**/
			int t = m_buffer.put(buf, r);
			if (t == -1)
			{
				/** Error during storing data to the buffer
				 *  (most probably problem with backing file).
				**/
				m_exception = true;
				m_error = errno;

				pthread_mutex_unlock(&m_mutex);
				break;
			}
			assert(t == r);
		}

		/** Signal that there are new data available (or error).
		**/
		pthread_cond_signal(&m_wakeupNewData);

		pthread_mutex_unlock(&m_mutex);
	}
}

