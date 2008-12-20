#include "FBuffer.hpp"
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <vector>

FBuffer::FBuffer(std::string path, int bufferSize) :
	CBuffer(bufferSize)
{
	std::string name = "/XXXXXX";
	std::vector<char> fullPath;

	fullPath.assign(path.begin(), path.end());
	fullPath.insert(fullPath.end(), name.begin(), name.end());

	m_fd = ::mkstemp(&fullPath[0]);
	::unlink(&fullPath[0]);
}

FBuffer::~FBuffer()
{
	close(m_fd);
}

int FBuffer::write(int pos, char *buf, int len)
{
	std::cerr << __PRETTY_FUNCTION__ << ", pos: " << pos << ", len: " << len << std::endl;

	return ::pwrite(m_fd, buf, len, pos);
}

int FBuffer::read(int pos, char *buf, int len)
{
	std::cerr << __PRETTY_FUNCTION__ << ", pos: " << pos << ", len: " << len << std::endl;

	return ::pread(m_fd, buf, len, pos);
}

