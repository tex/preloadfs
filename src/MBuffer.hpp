#ifndef FBUFFER_HPP
#define FBUFFER_HPP

#include "CBuffer.hpp"

/** Implements CBuffer's read/write methods. Uses memory buffer as
 *  a back storage for circular buffer.
**/
class MBuffer : public CBuffer
{
public:
	MBuffer(int bufferSize);
	~MBuffer();

private:
	int read(int pos, char *buf, int len);
	int write(int pos, char *buf, int len);

	/** Pointer to memory buffer.
	**/
	char *m_buffer;
};

#endif

