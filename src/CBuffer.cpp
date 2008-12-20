#include "CBuffer.hpp"
#include <errno.h>
#include <algorithm>
#include <iostream>

CBuffer::CBuffer(int bufferSize) :
	m_readP(0),
	m_writeP(0),
	m_full(false),
	m_empty(true),
	m_bufferSize(bufferSize)
{
}

void CBuffer::clear()
{
	m_readP = 0;
	m_writeP = 0;
	m_full = false;
	m_empty = true;
}

int CBuffer::free() const
{
	int free;

	if (m_full)
		free = 0;
	else if (m_empty)
		free = m_bufferSize;
	else if (m_readP < m_writeP)
		free = m_bufferSize - m_writeP + m_readP;
	else
		free = m_readP - m_writeP;

	std::cerr << __PRETTY_FUNCTION__ << std::endl;
	std::cerr << m_bufferSize << ", read: " << m_readP << ", write: " << m_writeP << " => " << free << std::endl;

	return free;
}

int CBuffer::full() const
{
	int full;

	full = m_bufferSize - free();

	std::cerr << __PRETTY_FUNCTION__ << std::endl;
	std::cerr << m_bufferSize << ", read: " << m_readP << ", write: " << m_writeP << " => " << full << std::endl;

	return full;
}

int CBuffer::get(char *buf, int len)
{
	char *orig_buf = buf;

	len = std::min(len, full());

	while (len > 0)
	{
		int r = read(m_readP, buf, std::min(len, m_bufferSize - m_readP));
		if (r == -1)
 			return -1;
		else if (r == 0)
			break;

		buf += r;
		len -= r;

		m_readP += r;
		if (m_readP >= m_bufferSize)
			m_readP -= m_bufferSize;
		if (m_readP == m_writeP)
			m_empty = true;
		m_full = false;
	}
	return buf - orig_buf;
}

int CBuffer::put(char *buf, int len)
{
	char *orig_buf = buf;

	len = std::min(len, free());

	while (len > 0)
	{
		int r = write(m_writeP, buf, std::min(len, m_bufferSize - m_writeP));
		if (r == -1)
			return -1;
		else if (r == 0)
			break;

		buf += r;
		len -= r;

		m_writeP += r;
		if (m_writeP >= m_bufferSize)
			m_writeP -= m_bufferSize;
		if (m_readP == m_writeP)
			m_full = true;
		m_empty = false;
	}
	return buf - orig_buf;
}

