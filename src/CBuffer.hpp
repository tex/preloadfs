/** Circular buffer, virtual class; read/write functions for
 *  reading/writting data from/to storage must be implemented
 *  by class that inherits from this one.
 *
 *  License: GPLv2
 *  (c) Milan Svoboda, 2008
**/

#include <ostream>

class CBuffer
{
public:
	/** Constructor
	 *  @param bufferSize
	**/
	CBuffer(int bufferSize);
	virtual ~CBuffer() { };

	/** Clear the buffer, set write and read pointer to
	 *  the begining. Buffer is empty and not full.
	**/
	void clear();

	/** Append data to the buffer.
	 *  @return size of data appended to the buffer in bytes
	 **/
	int put(char *buf, int len);

	/** Get data from the buffer.
	 *  @return size of data read from the buffer in bytes
	 **/
	int get(char *buf, int len);

	/** Return size of free buffer space in bytes.
	 *  @return size of free buffer space in bytes
	 **/
	int free() const;

	bool isFree() const;

	/** Return size of occupied buffer space in bytes.
	 *  @return size of occupied buffer space in bytes
	**/
	int full() const;

	bool isFull() const;

	bool isAlmostFull() const;

	/** Return size of buffer in bytes.
	 *  @return size of buffer in bytes
	 **/
	int size() const;

	void advance(int offset);

private:
	/** Really read data from backed storage (may be memory,
	 *  file or ...). This method must be implemented by
	 *  a class that inherits from this one (CBuffer).
	 *  @param pos position
	 *  @param buf pointer to buffer
	 *  @param len length of required data
	 *  @return size of read data
	**/
	virtual int read(int pos, char *buf, int len) = 0;

	/** Really write data to backed storage (may be memory,
	 *  file or ...). This method must be implemented by
	 *  a class that inherits from this one (CBuffer).
	 *  @param pos position
	 *  @param buf pointer to buffer
	 *  @param len length of required data
	 *  @return size of written data
	**/
	virtual int write(int pos, char *buf, int len) = 0;

	/** Pointer to read start
	**/
	int m_readP;

	/** Pointer to write start
	**/
	int m_writeP;

	/** If true, buffer is full
	**/
	bool m_full;

	/** If true, buffer is empty
	**/
	bool m_empty;

	/** Contains size of the buffer in bytes
	**/
	int m_bufferSize;
};

