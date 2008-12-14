#include <assert.h>
#include <sys/fsuid.h>
#include <dirent.h>
#include <sys/types.h>
#include <string.h>
#include <sched.h>
#include <errno.h>

#include <cstdlib>
#include <iostream>

#include "FuseHttp.hpp"

extern bool g_DebugMode;

FuseHttp::FuseHttp()
{
	memset(&m_ops, 0, sizeof m_ops);

	m_ops.init = FuseHttp::init;
	m_ops.destroy = FuseHttp::destroy;

	m_ops.readlink = FuseHttp::readlink;
	m_ops.getattr = FuseHttp::getattr;
	m_ops.readdir = FuseHttp::readdir;
	m_ops.mknod = FuseHttp::mknod;
	m_ops.mkdir = FuseHttp::mkdir;
	m_ops.rmdir = FuseHttp::rmdir;
	m_ops.unlink = FuseHttp::unlink;
	m_ops.symlink = FuseHttp::symlink;
	m_ops.rename = FuseHttp::rename;
	m_ops.link = FuseHttp::link;
	m_ops.chmod = FuseHttp::chmod;
	m_ops.chown = FuseHttp::chown;
	m_ops.truncate = FuseHttp::truncate;
	m_ops.utime = FuseHttp::utime;
	m_ops.open = FuseHttp::open;
	m_ops.read = FuseHttp::read;
	m_ops.write = FuseHttp::write;
	m_ops.flush = FuseHttp::flush;
	m_ops.release = FuseHttp::release;
	m_ops.fsync = FuseHttp::fsync;
	m_ops.statfs = FuseHttp::statfs;
}

FuseHttp::~FuseHttp()
{
}

int FuseHttp::Run(int argc, const char **argv)
{
	// Note: Remove const from argv to make a compiler
	// happy. Anyway, it's used as constant in fuse_main()
	// and the decalration of fuse_main  should be fixed
	// in the fuse package.
	//
	return fuse_main(argc, const_cast<char **> (argv),  &m_ops);
}

void *FuseHttp::init(void)
{
	return NULL;
}

void FuseHttp::destroy(void *data)
{
}

int FuseHttp::getattr(const char *name, struct stat *st)
{
	int	 r = 0;
	CFile	*file;

	name = getpath(name);

	// Speed optimization: Fast path for '.' questions.
	//
	if ((name[0] == '.') && (name[1] == '\0'))
	{
		return ::lstat(name, st);
	}
	
	file = g_FileManager->Get(name);
	if (!file)
		return -errno;

	file->Lock();
	
	if (file->getattr(name, st) == -1)
		r = -errno;

	file->Unlock();

	g_FileManager->Put(file);
	
	return r;
}

/**
 * Note:
 *       fi->flags contains open flags supplied with user. Fuse lib and kernel
 *       take care about correct read / write (append) behaviour. We don't need to
 *       care about this. Fuse also takes care about permissions.
 */
int FuseHttp::open(const char *name, struct fuse_file_info *fi)
{
	int	 r = 0;
	CFile	*file;
	
	name = getpath(name);

	file = g_FileManager->Get(name);

	rDebug("FuseHttp::open %p name: %s", (void *) file, name);

	if (!file)
		return -errno;

	file->Lock();
	
	if (file->open(name, fi->flags) == -1)
		r = -errno;

	fi->fh = (long) file;

	file->Unlock();

	if (r != 0)
	{
		// Error occured during open. We must put file to keep
		// reference number in sync. (Release won't be called if
		// open returns error code.)
		//
		g_FileManager->Put(file);
	}
	
	// There is no g_FileManager->Put(file), because the file is
	// opened and is beeing used...
	// 
	return r;
}

int FuseHttp::read(const char *name, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	int	 r;
	CFile	*file = reinterpret_cast<CFile *> (fi->fh);

	rDebug("FuseHttp::read(B) %p name: %s, size: 0x%x, offset: 0x%llx",
			(void *) file, getpath(name), (unsigned int) size, (long long int) offset);

	file->Lock();
	
	r = file->read(buf, size, offset);
	if (r == -1)
		r = -errno;

	file->Unlock();

	rDebug("FuseHttp::read(E) %p name: %s, size: 0x%x, offset: 0x%llx, returned: 0x%x",
			(void *) file, getpath(name), (unsigned int) size, (long long int) offset, r);

	sched_yield();

	return r;
}

int FuseHttp::write(const char *name, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	int	 r;
	CFile	*file = reinterpret_cast<CFile *> (fi->fh);

	rDebug("FuseHttp::write %p name: %s, size: 0x%x, offset: 0x%llx",
			(void *) file, getpath(name), (unsigned int) size, (long long int) offset);

	file->Lock();
	
	r = file->write(buf, size, offset);
	if (r == -1)
		r = -errno;

	file->Unlock();

	sched_yield();

	return r;
}

/**
 * Counterpart to the open, for each open there will be exactly
 * one call for release.
 */
int FuseHttp::release(const char *name, struct fuse_file_info *fi)
{
	int	 r = 0;
	CFile	*file = reinterpret_cast<CFile *> (fi->fh);

	name = getpath(name);
	rDebug("FuseHttp::release %p name: %s", (void *) file, name);

	file->Lock();
	
	if (file->release(name) == -1)
		r = -errno;

	file->Unlock();

	g_FileManager->Put(file);
	
	return r;
}

int FuseHttp::readlink(const char *name, char *buf, size_t size)
{
	return 0;
}
	
int FuseHttp::readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	return 0;
}

int FuseHttp::mknod(const char *name, mode_t mode, dev_t rdev)
{
	return 0;
}

int FuseHttp::mkdir(const char *path, mode_t mode)
{
	return 0;
}

int FuseHttp::rmdir(const char *path)
{
	return 0;
}

int FuseHttp::unlink(const char *path)
{
	return 0;
}

int FuseHttp::symlink(const char *from, const char *to)
{
	return 0;
}

int FuseHttp::rename(const char *from, const char *to)
{
	return 0;
}

int FuseHttp::link(const char *from, const char *to)
{
	return 0;
}

int FuseHttp::chmod(const char *path, mode_t mode)
{
	return 0;
}

int FuseHttp::chown(const char *path, uid_t uid, gid_t gid)
{
	return 0;
}

int FuseHttp::truncate(const char *name, off_t size)
{
	return 0;
}

int FuseHttp::utime(const char *name, struct utimbuf *buf)
{
	return 0;
}

int FuseHttp::flush(const char *name, struct fuse_file_info *fi)
{
	return 0;
}

int FuseHttp::fsync(const char *name, int isdatasync,
                          struct fuse_file_info *fi)
{
	return 0;
}

int FuseHttp::statfs(const char *name, struct statvfs *buf)
{
	return 0;
}

