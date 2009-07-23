#ifndef DEVICEFILE_HPP
#define DEVICEFILE_HPP

#include "Device.hpp"

class DeviceFile : public Device
{
public:
	bool open(const char *name);
	ssize_t pread(char *buf, size_t len, off_t offset);
	off_t size();
	void cancel() { };

private:
	int m_fd;
};

#endif

