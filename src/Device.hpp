#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <sys/types.h>

class Device
{
public:
	static Device *deviceFactory(const char *name);

	virtual bool open(const char *name) = 0;
	virtual ssize_t pread(char *buf, size_t len, off_t offset) = 0;
	virtual off_t size() = 0;
	virtual void cancel() = 0;
};


#endif

