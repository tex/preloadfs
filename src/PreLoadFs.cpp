#include "PreLoadFs.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>
#include <string.h>
#include <iostream>

extern bool g_DebugMode;

PreLoadFs::PreLoadFs(std::string tmpPath, size_t tmpSize, std::string fileToMount) :
	m_name(fileToMount),
	m_refs(0),
	m_offset(0),
	m_buffer(tmpPath, tmpSize),
	m_exception(false),
	m_seeked(false)
{
}

PreLoadFs::~PreLoadFs()
{
}

void *PreLoadFs::init()
{
	int r = pthread_create(&m_thread, NULL, &PreLoadFs::runT, this);
	if (r != 0)
		exit(EXIT_FAILURE);
	return NULL;
}

void PreLoadFs::destroy(void *arg)
{
	/** Cancel thread and wait to its termination.
	**/
	pthread_cancel(m_thread);
	pthread_join(m_thread, NULL);

	assert(m_refs == 0);
}

int PreLoadFs::getattr(const char *name, struct stat *st)
{
	int r = 0;

	memset(st, 0, sizeof(struct stat));

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
		std::cout << __PRETTY_FUNCTION__ << std::endl;

	/** File name of the mounted file.
	**/
	boost::filesystem::path tmp(m_name.leaf());

	/** Allow to open only our mounted file.
	**/
	if (tmp.string().compare(&name[1]) != 0)
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

/**
 * m_mutex must be locked
**/
void PreLoadFs::seek(off_t offset)
{
	assert(m_offset != offset);

	if (g_DebugMode)
		std::cout << __PRETTY_FUNCTION__ << " " << offset << std::endl;

	if ((m_offset < offset) && (m_offset + m_buffer.full() > offset))
	{
		if (g_DebugMode)
			std::cout << __PRETTY_FUNCTION__ << " fast m_offset:" << m_offset
			          << ", offset:" << offset << ", ->" << m_offset + m_buffer.full() << std::endl;

		/** There are data in the buffer covering required offset.
		**/
		assert(offset - m_offset > 0);
		m_buffer.advance(offset - m_offset);
	}
	else
	{
		/** Clear the circular buffer.
		**/
		m_buffer.clear();

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
	}

	/** Set offset to new value.
	**/
	m_offset = offset;

	/** Let know the thread that it shall read new data.
	**/
	pthread_cond_signal(&m_wakeupReadNewData);
}

int PreLoadFs::read(const char *name, char *buf, size_t len, off_t offset, struct fuse_file_info * /*fi*/)
{
	char *orig_buf = buf;

	if (g_DebugMode)
		std::cout << __PRETTY_FUNCTION__<< "offset: " << offset
		          << ", len: " << len << ", m_offset: " << m_offset << std::endl;

	/** File name of the mounted file.
	**/
	boost::filesystem::path tmp(m_name.leaf());

	/** Assert that user can read only from our mounted file.
	**/
	assert(tmp.string().compare(&name[1]) == 0);

	pthread_mutex_lock(&m_mutex);

	/** Seek if user wants to read from offset different than we currently have.
	**/
	if (m_offset != offset)
		seek(offset);

	while (len > 0)
	{
		if (g_DebugMode)
			m_buffer.stats();

		while (m_buffer.isFree() && (m_exception == false))
		{
			/** Wait for a new data if buffer is empty or exception
			 *  is detected (when exception is detected there will be no more
			 *  data so we have to read what's available because there will
			 *  not be any new data.
			**/
			pthread_cond_wait(&m_wakeupNewData, &m_mutex);
		}

		/** Read data from buffer.
		**/
		int r = m_buffer.get(buf, len);

		if (g_DebugMode)
			std::cout << __PRETTY_FUNCTION__ << ", get returned: " << r << " (" << strerror(errno) << ")" << std::endl;

		if (r == -1)
			break;

		buf += r;
		len -= r;

		/** Advance offset.
		**/
		m_offset += r;

		/** Detect exception only if there are no data in the buffer.
		**/
		if (r == 0)
		{
			if (m_exception == true)
			{
				if (m_error != 0)
				{
					/** Error detected when read...
					**/
					pthread_mutex_unlock(&m_mutex);

					assert(m_error > 0);
					return -m_error;
				}

				/** Just end of the file...
				**/
				break;
			}
		}

		/** Let know the thread that it can read new data.
		**/
		pthread_cond_signal(&m_wakeupReadNewData);
	}
	pthread_mutex_unlock(&m_mutex);

	/** Compute total bytes read.
	**/
	int t = buf - orig_buf;

	if (g_DebugMode)
		std::cout << __PRETTY_FUNCTION__ << ", total ret: " << t << std::endl;

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
	off_t offset = 0;
	int   buf_size = std::min(64 * 1024, m_buffer.size());
	char* buf = new char[buf_size];

	/** We are read only buffering 'filesystem'. So we open
	 *  a file as read only. And we do not care about closing
	 *  it because the if this thread gets killed the
	 *  whole application is going to end.
	**/
	int fd = ::open(m_name.string().c_str(), O_RDONLY);

	pthread_mutex_lock(&m_mutex);

	if (fd == -1)
	{
		m_exception = true;
		m_error = errno;

		/** Signal that there is an error.
		**/
		pthread_cond_signal(&m_wakeupNewData);
	}

	while (true)
	{
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
		{
			m_seeked = false;
			offset = m_offset;
		}

		int readBytes = std::min(buf_size, m_buffer.free());

		pthread_mutex_unlock(&m_mutex);

		/** This read() may take a long time, thus we don't hold
		 *  the mutex.
		**/
		if (g_DebugMode)
			std::cout << __PRETTY_FUNCTION__ << "..reading: " << readBytes << std::endl;

		int r = ::pread(fd, buf, readBytes, offset);

		if (g_DebugMode)
			std::cout << __PRETTY_FUNCTION__ << "..read: " << r << std::endl;

		pthread_mutex_lock(&m_mutex);

		if (m_seeked == true)
		{
			if (g_DebugMode)
				std::cout << __PRETTY_FUNCTION__ << "..seeked...continue" << std::endl;

			m_seeked = false;
			offset = m_offset;

			/** User seeked before we stored data in the buffer,
			 *  by continue we discard it and start again.
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
			offset += r;

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

				/** Exit this thread.
				**/
				break;
			}
			assert(t == r);
		}
		/** Signal that there are new data available (or error).
		**/
		pthread_cond_signal(&m_wakeupNewData);
	}
	pthread_mutex_unlock(&m_mutex);

	delete buf;
}

