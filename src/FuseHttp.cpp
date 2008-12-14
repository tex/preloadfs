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
	std::cerr << __PRETTY_FUNCTION__ << ", name: " << name << std::endl;

	if (strstr(name, ".mp3") || strstr(name, "mp4") || strstr(name, ".avi"))
	{
		st->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
	}
	else
	{
		st->st_mode = S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
	}

	return 0;
}

int FuseHttp::open(const char *name, struct fuse_file_info *fi)
{
	int r = 0;

	std::cerr << __PRETTY_FUNCTION__ << ", name: " << name << std::endl;

	return r;
}

int FuseHttp::read(const char *name, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	int r = 0;

	sched_yield();

	return r;
}

int FuseHttp::release(const char *name, struct fuse_file_info *fi)
{
	int r = 0;
	return r;
}

int FuseHttp::write(const char *name, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	return 0;
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

