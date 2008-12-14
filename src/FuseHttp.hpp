#ifndef FUSEHTTP_H
#define FUSEHTTP_H

#include <fuse.h>

class FuseHttp
{
	static int readlink (const char *, char *, size_t);
	static int getattr (const char *, struct stat *);
	static int getdir (const char *, fuse_dirh_t, fuse_dirfil_t);
	static int mknod (const char *, mode_t, dev_t);
	static int mkdir (const char *, mode_t);
	static int unlink (const char *);
	static int rmdir (const char *);
	static int symlink (const char *, const char *);
	static int rename (const char *, const char *);
	static int link (const char *, const char *);
	static int chmod (const char *, mode_t);
	static int chown (const char *, uid_t, gid_t);
	static int truncate (const char *, off_t);
	static int utime (const char *, struct utimbuf *);
	static int open (const char *, struct fuse_file_info *);
	static int read (const char *, char *, size_t, off_t, struct fuse_file_info *);
	static int write (const char *, const char *, size_t, off_t,struct fuse_file_info *);
	static int flush (const char *, struct fuse_file_info *);
	static int release (const char *, struct fuse_file_info *);
	static int fsync (const char *, int, struct fuse_file_info *);
	static int setxattr (const char *, const char *, const char *, size_t, int);
	static int getxattr (const char *, const char *, char *, size_t);
	static int listxattr (const char *, char *, size_t);
	static int removexattr (const char *, const char *);
	static int opendir (const char *, struct fuse_file_info *);
	static int readdir (const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
	static int releasedir (const char *, struct fuse_file_info *);
	static int fsyncdir (const char *, int, struct fuse_file_info *);
	static void *init (void);
	static void  destroy (void *);
	static int access (const char *, int);
	static int create (const char *, mode_t, struct fuse_file_info *);
	static int ftruncate (const char *, off_t, struct fuse_file_info *);
	static int fgetattr (const char *, struct stat *, struct fuse_file_info *);
	static int statfs (const char *, struct statvfs *);

	struct fuse_operations m_ops;

public:
	FuseHttp();
	~FuseHttp();
	
	int Run(int argc, const char **argv);
};

#endif

