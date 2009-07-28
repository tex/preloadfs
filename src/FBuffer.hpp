#ifndef FBUFFER_HPP
#define FBUFFER_HPP

#include "CBuffer.hpp"
#include <string>

/** Implements CBuffer's read/write methods. Uses regular file as
 *  a back storage for circular buffer.
**/
class FBuffer : public CBuffer
{
public:
	FBuffer(const std::string& tmpPath, int bufferSize);
	~FBuffer();

private:
	int read(int pos, char *buf, int len);
	int write(int pos, char *buf, int len);

	/** Back storage's file descriptor.
	**/
	int m_fd;
};

#endif

