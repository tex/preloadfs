#ifndef PRELOADFS_HPP
#define PRELOADFS_HPP

#include "MBuffer.hpp"
#include <fuse.h>
#include <pthread.h>
#include <boost/filesystem.hpp>

class PreLoadFs
{
public:
	/** Constructor.
	 *  @param tmpPath temporary file storage path
	 **/
	PreLoadFs(const std::string& tmpPath, size_t tmpSize, const std::string& fileToMount);
	~PreLoadFs();

	void *init();
	void destroy(void *arg);

	int getattr (const char *, struct stat *);
	int readdir (const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);

	int open(const char *name, struct fuse_file_info *fi);
	int release(const char *name, struct fuse_file_info *fi);
	int read(const char *name, char *buf, size_t len, off_t offset, struct fuse_file_info *fi);
	int write(const char *name, const char *buf, size_t len, off_t offset, struct fuse_file_info *fi);

private:
	int stat(char *buf, size_t len);

	/** "Trampoline" function just to execute run() in
	 *  correct context.
	**/
	static void *runT(void *arg);

	/** Main thread function. Reads a file in advance and
	 *  caches it into a buffer.
	**/
	void run();

	void seek(off_t offset);

	/** Name of pre-loaded (mounted) file.
	**/
	boost::filesystem::path m_name;

	/** Reference counter
	**/
	int             m_refs;

	/** Read offset. Used to be able to determine wheter seek
	 *  is required or not.
	 **/
	off_t           m_offset;

	/** Buffer.
	**/
	MBuffer         m_buffer;

	/** Thread used to pre-read content of the file.
	**/
	pthread_t       m_thread;

	/** Condition variable used to wake-up thread to let it
	 *  continue to read new data.
	 **/
	pthread_cond_t  m_wakeupReadNewData;

	/** Confition variable used to signal that new data
	 *  has been read.
	 **/
	pthread_cond_t  m_wakeupNewData;

	pthread_cond_t	m_wakeupStatAvailable;

	/** Mutex that protects every variables shared with thread.
	**/
	pthread_mutex_t m_mutex;

	/** True if error or end of file has been detected
	 *  during read.
	**/
	bool            m_exception;

	/** Status (error) code. Valid only if m_exception is true.
	 *  When end of file detected this variable has value of 0.
	 *  Otherwise it contains error code.
	 **/
	int             m_error;

	/** True if user seeked. Set only in read() and
	 *  cleared only in run().
	 **/
	bool            m_seeked;

	off_t		m_size;
};

#endif

