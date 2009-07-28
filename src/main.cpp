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

#include <boost/program_options.hpp>

namespace po = boost::program_options;

bool       g_DebugMode = false;
PreLoadFs* g_PreLoadFs;

void print_license()
{
	printf("%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
	printf("Copyright (C) 2008,2009  Milan Svoboda.\n");
	printf("This is free software; see the source for copying conditions.  There is NO\n");
	printf("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n");
}

void *Init()
{
	return g_PreLoadFs->init();
}

void Destroy(void *arg)
{
	return g_PreLoadFs->destroy(arg);
}

int Getattr(const char *name, struct stat *stat)
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

int run(std::vector<const char *>& fuse_c_str, const std::string& fileToMount, const std::string& tmpPath, int bufSize)
{
	g_PreLoadFs = new PreLoadFs(tmpPath, bufSize, fileToMount);
	if (g_PreLoadFs == NULL)
	{
		std::cerr << "Failed to create an instance of PreLoadFs" << std::endl;
		return EXIT_FAILURE;
	}

	struct fuse_operations ops;
	memset(&ops, 0, sizeof ops);

	ops.init = Init;
	ops.destroy = Destroy;
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

	std::string fileToMount;
	std::string mountPoint;
	std::string tmpPath = "/tmp";
	int bufSize = 128;

	po::options_description desc("Usage: " PACKAGE " [options] fileToMount mountPath\n" "\nOptions");
	desc.add_options()
		("fileToMount", po::value<std::string>(&fileToMount), "file to mount (local file or HTTP URL)")
		("mountPoint", po::value<std::string>(&mountPoint), "mount point")
		("tmp,t", po::value<std::string>(&tmpPath), "temporary path for a buffer")
		("buffer,b", po::value<int>(&bufSize), "buffer size in KiB")
		("debug,d", "turn on debug mode")
		("help,h", "print this help")
		("version,v", "print version")
	;

	po::positional_options_description pdesc;
	pdesc.add("fileToMount", 1);
	pdesc.add("mountPoint", 1);

	po::variables_map vm;
	try {
		po::store(po::command_line_parser(argc, argv).options(desc).positional(pdesc).run(), vm);
	} catch (...) {
		std::cout << desc;;
		exit(EXIT_FAILURE);
	}
	po::notify(vm);

	if (vm.count("help"))
	{
		std::cout << desc;
		exit(EXIT_SUCCESS);
	}
	if (vm.count("version"))
	{
		print_license();
		exit(EXIT_SUCCESS);
	}
	if (vm.count("debug"))
	{
		g_DebugMode = true;
	}
	if (fileToMount.empty())
	{
		std::cout << "fileToMount not set!\n" << desc;
		exit(EXIT_FAILURE);
	}
	if (mountPoint.empty())
	{
		std::cout << "mountPoint not set!\n" << desc;
		exit(EXIT_FAILURE);
	}

	fuse_c_str.push_back(argv[0]);

	if (g_DebugMode)
		fuse_c_str.push_back("-f");
	fuse_c_str.push_back("-o");
	fuse_c_str.push_back("default_permissions,use_ino");
	fuse_c_str.push_back(mountPoint.c_str());

	chdir(mountPoint.c_str());

	return run(fuse_c_str, fileToMount, tmpPath, bufSize * 1024);
}

