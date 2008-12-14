#include "config.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include "FuseHttp.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>

bool g_DebugMode;

void print_license()
{
	printf("%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
	printf("Copyright (C) 2008  Milan Svoboda.\n");
	printf("This is free software; see the source for copying conditions.  There is NO\n");
	printf("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n");
}

void print_help(const char *name)
{
	printf("%s mount_point\n", name);
	exit(1);
}

int main(int argc, char **argv)
{
	g_DebugMode = true;

	if (argc != 2)
		print_help(argv[0]);

	std::string dirMount(argv[1]);

	std::vector<std::string> fuseOptions;
	fuseOptions.push_back(argv[0]);

	// Set up default options for fuse.
	// 
	// Fuse problems:
	// 	kernel_cache - causes sigfaults when trying to run compiled
	// 	               executables from FuseCompressed filesystem
	//
	if (g_DebugMode)
		fuseOptions.push_back("-f");
	fuseOptions.push_back("-o");
	fuseOptions.push_back("default_permissions,use_ino,kernel_cache");
	fuseOptions.push_back(dirMount);

	std::vector<const char *> fuse_c_str;
	for (unsigned int i = 0; i < fuseOptions.size(); ++i)
	{
		fuse_c_str.push_back((fuseOptions[i].c_str()));
	}

	FuseHttp fusehttp;

	umask(0);
	return fusehttp.Run(fuse_c_str.size(), &fuse_c_str[0]);
}

