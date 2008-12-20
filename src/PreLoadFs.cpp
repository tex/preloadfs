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

PreLoadFs::PreLoadFs(std::string tmpPath, size_t tmpSize, std::string fileToMount) :
	m_name(fileToMount),
	m_refs(0),
	m_offset(0),
	m_buffer(tmpPath, tmpSize),
	m_exception(false),
	m_seeked(false)
{
	/** We are read only buffering 'filesystem'. So we open
	 *  a file as read only.
	**/
	m_fd = ::open(m_name.string().c_str(), O_RDONLY);

	if (m_fd != -1)
	{
		/** Create only one thread
		**/
		int r = pthread_create(&m_thread, NULL, &PreLoadFs::runT, this);
		if (r != 0)
		{
			/** Failed to create thread. Clean up
			 *  used resources.
			**/
			::close(m_fd);
			m_fd = -1;
		}
	}
}

PreLoadFs::~PreLoadFs()
{
	/** Cancel thread and wait to its termination.
	**/
	pthread_cancel(m_thread);
	pthread_join(m_thread, NULL);

	close(m_fd);
	m_fd = -1;

	assert(m_refs == 0);
	assert(m_fd == -1);
}

int PreLoadFs::getattr(const char *name, struct stat *st)
{
	int r = 0;

	std::cout << __PRETTY_FUNCTION__ << name << std::endl;

	memset(st, 0, sizeof(struct stat));

	assert(name[0] != '\0');
	if (name[1] == '\0')
		st->st_mode = S_IFDIR | 0755;
	else
	{
		++name;
		boost::filesystem::path tmp(m_name.leaf());
		std::cout << __PRETTY_FUNCTION__ << tmp << std::endl;

		if (strcmp(name, tmp.string().c_str()) == 0)
			r = ::stat(m_name.string().c_str(), st);
		else
			r = -ENOENT;
	}

	return r;
}

int PreLoadFs::readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	struct stat st;

	memset(&st, 0, sizeof(st));
	st.st_ino = 0;
	st.st_mode = S_IFDIR;
	filler(buf, ".", &st, 0);

	memset(&st, 0, sizeof(st));
	st.st_ino = 1;
	st.st_mode = S_IFDIR;
	filler(buf, "..", &st, 0);

	memset(&st, 0, sizeof(st));
	st.st_ino = 2;
	st.st_mode = S_IFREG;
	boost::filesystem::path tmp(m_name.leaf());
	filler(buf, tmp.string().c_str(), &st, 0);

	return 0;
}

int PreLoadFs::open(const char *name, struct fuse_file_info * /*fi*/)
{
	/** Use only one file descriptor for all user's open requests.
	**/
	boost::filesystem::path tmp(m_name.leaf());
	assert(strcmp(name[1], tmp.string().c_str) == 0);

	if (m_fd != -1)
		++m_refs;
	return m_fd;
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

int PreLoadFs::read(const char *name, char *buf, size_t len, off_t offset, struct fuse_file_info * /*fi*/)
{
	char *orig_buf = buf;

	/** Perform seek if user wants to read from offset
	 *  different than we have currently.
	**/
	if (m_offset != offset)
	{
		pthread_mutex_lock(&m_mutex);

		/** Clear the circular buffer.
		**/
		m_buffer.clear();

		/** Seek to required position in opened file.
		**/
		off_t r = ::lseek(m_fd, offset, SEEK_SET);
		if (r == (off_t) -1)
		{
			/** lseek() failed, unlock mutex and return
			 *  error return value.
			**/
			pthread_mutex_unlock(&m_mutex);
			return -1;
		}

		/** Set offset to new value.
		**/
		m_offset = r;

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

		/** Let know the thread that it can read new data.
		**/
		pthread_cond_signal(&m_wakeupReadNewData);

		pthread_mutex_unlock(&m_mutex);
	}

	while (len > 0)
	{
		pthread_mutex_lock(&m_mutex);

		while (m_buffer.isFree())
		{
			/** Buffer is empty. Check exception flag and
			 *  return error code in case of error or number
			 *  of bytes read so far in case of detected end
			 *  of file.
			**/
			if (m_exception == true)
			{
				pthread_mutex_unlock(&m_mutex);

				if (m_error != 0)
				{
					/** Error. Set errno to let the user know
					 *  type of error.
					**/
					errno = m_error;

					return -1;
				}
				else
					goto end;
			}

			/** Wait for a new data.
			**/
			pthread_cond_wait(&m_wakeupNewData, &m_mutex);
		}

		/** Read data from buffer.
		**/
		int r = m_buffer.get(buf, len);
		buf += r;
		len -= r;

		/** Let know the thread that it can read new data.
		**/
		pthread_cond_signal(&m_wakeupReadNewData);

		pthread_mutex_unlock(&m_mutex);
	}
end:
	/** Compute total bytes read.
	**/
	int t = buf - orig_buf;

	/** Advance offset.
	**/
	m_offset += t;

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
	/** Local buffer size at most 4096 bytes
	**/
	int buf_size = std::min(4096, m_buffer.size());
	char* buf = new char[buf_size];

	while (true)
	{
begin:		int freeBytes = 0;

		pthread_mutex_lock(&m_mutex);

		/** Wait until buffer is not full or exception is resolved.
		**/
		while (m_buffer.isFull() || (m_exception == true))
			pthread_cond_wait(&m_wakeupReadNewData, &m_mutex);

		/** Get number of bytes that may be stored in the buffer.
		**/
		freeBytes = m_buffer.free();

		pthread_mutex_unlock(&m_mutex);

		/** This read() may take a long time, thus we don't hold
		 *  the mutex.
		**/
		int r = ::read(m_fd, buf, std::min(buf_size, freeBytes));

		pthread_mutex_lock(&m_mutex);

		if (m_seeked == true)
		{
			m_seeked = false;

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
			char *tmp = buf;

			while (r > 0)
			{
				while (m_buffer.isFull())
				{
					/** Buffer is full, no space to store new data.
					**/

					if (m_seeked == true)
					{
						m_seeked = false;

						/** User seeked before we stored data in the buffer,
						 *  by continue we discard them and start again.
						**/
						goto begin;
					}

					/** Wait until there is a free space in the
					 *  buffer.
					**/
					pthread_cond_wait(&m_wakeupReadNewData, &m_mutex);
				}

				/** Store data to the buffer.
				**/
				int t = m_buffer.put(tmp, r);
				assert(t != 0);

				if (t == -1)
				{
					/** Error during storing data to the buffer
					 *  (most probably problem with backing file).
					**/
					m_exception = true;
					m_error = errno;
					break;
				}
				else
				{
					tmp += t;
					r   -= t;
				}
			}
		}

		/** Signal that there are new data available (or error).
		**/
		pthread_cond_signal(&m_wakeupNewData);

		pthread_mutex_unlock(&m_mutex);
	}
}

