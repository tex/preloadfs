#include "FBuffer.hpp"
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <iostream>

FBuffer::FBuffer(const std::string& tmpPath, int bufferSize) :
	CBuffer(bufferSize)
{
	std::string name = "/XXXXXX";
	std::vector<char> fullPath;

	fullPath.assign(tmpPath.begin(), tmpPath.end());
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
	return ::pwrite(m_fd, buf, len, pos);
}

int FBuffer::read(int pos, char *buf, int len)
{
	return ::pread(m_fd, buf, len, pos);
}

