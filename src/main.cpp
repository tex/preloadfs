#include "config.h"

#include "PreLoadFs.hpp"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>

bool       g_DebugMode;
PreLoadFs* g_PreLoadFs;

void print_license()
{
	printf("%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
	printf("Copyright (C) 2008  Milan Svoboda.\n");
	printf("This is free software; see the source for copying conditions.  There is NO\n");
	printf("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n");
}

void print_help(const char *name)
{
	printf("%s fileToMount mountPoint tmpPath bufSize(in KiB)\n", name);
	exit(EXIT_FAILURE);
}

int Getattr (const char *name, struct stat *stat)
{
	return g_PreLoadFs->getattr(name, stat);
}

int Readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	return g_PreLoadFs->readdir(path, buf, filler, offset, fi);
}

int Open(const char *name, struct fuse_file_info *fi)
{
	return g_PreLoadFs->open(name, fi);
}

int Release(const char *name, struct fuse_file_info *fi)
{
	return g_PreLoadFs->release(name, fi);
}

int Read(const char *name, char *buf, size_t len, off_t offset, struct fuse_file_info *fi)
{
	return g_PreLoadFs->read(name, buf, len, offset, fi);
}

int run(std::vector<const char *>& fuse_c_str, const char* fileToMount, const char* tmpPath, int bufSize)
{
	g_PreLoadFs = new PreLoadFs(tmpPath, bufSize, fileToMount);
	if (g_PreLoadFs == NULL)
	{
		std::cerr << "Failed to create an instance of PreLoadFs" << std::endl;
		return EXIT_FAILURE;
	}

	struct fuse_operations ops;
	memset(&ops, 0, sizeof ops);

	ops.getattr = Getattr;
	ops.readdir = Readdir;
	ops.open = Open;
	ops.read = Read;
	ops.release = Release;
 
	return fuse_main(fuse_c_str.size(), const_cast<char**>(&fuse_c_str[0]), &ops);
}

int main(int argc, char **argv)
{
	std::vector<const char *> fuse_c_str;

	g_DebugMode = true;

	if (argc != 5)
		print_help(argv[0]);

	const char* fileToMount = argv[1];
	const char* mountPoint = argv[2];
	const char* tmpPath = argv[3];
	int bufSize = atoi(argv[4]) * 1024;

	fuse_c_str.push_back(argv[0]);

	if (g_DebugMode)
		fuse_c_str.push_back("-f");
	fuse_c_str.push_back("-o");
	fuse_c_str.push_back("default_permissions,use_ino");
	fuse_c_str.push_back(mountPoint);

	chdir(mountPoint);

	return run(fuse_c_str, fileToMount, tmpPath, bufSize);
}

