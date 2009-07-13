#include "MBuffer.hpp"
#include <string.h>

MBuffer::MBuffer(std::string, int bufferSize) :
	CBuffer(bufferSize)
{
	m_buffer = new char[bufferSize];
}

MBuffer::~MBuffer()
{
	delete m_buffer;
}

int MBuffer::write(int pos, char *buf, int len)
{
	memcpy(m_buffer + pos, buf, len);
	return len;
}

int MBuffer::read(int pos, char *buf, int len)
{
	memcpy(buf, m_buffer + pos, len);
	return len;
}

