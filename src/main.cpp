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
	printf("%s fileToMount mountPoint\n", name);
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

int run(std::vector<const char *>& fuse_c_str, std::string& fileToMount)
{
	g_PreLoadFs = new PreLoadFs("/tmp", 1 * 1024 * 1024, fileToMount);
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
	g_DebugMode = true;

	if (argc != 3)
		print_help(argv[0]);

	std::string fileToMount(argv[1]);
	std::string mountPoint(argv[2]);

	std::vector<std::string> fuseOptions;
	fuseOptions.push_back(argv[0]);

	if (g_DebugMode)
		fuseOptions.push_back("-f");
	fuseOptions.push_back("-o");
	fuseOptions.push_back("default_permissions,use_ino,kernel_cache");
	fuseOptions.push_back(mountPoint);

	std::vector<const char *> fuse_c_str;
	for (unsigned int i = 0; i < fuseOptions.size(); ++i)
	{
		fuse_c_str.push_back((fuseOptions[i].c_str()));
	}

	chdir(mountPoint.c_str());
	umask(0);

	return run(fuse_c_str, fileToMount);
}

