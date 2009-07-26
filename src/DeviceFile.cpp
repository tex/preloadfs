#include "DeviceFile.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

bool DeviceFile::open(const char *name)
{
	m_fd = ::open(name, O_RDONLY);
	if (m_fd == -1)
		return false;
	return true;
}

ssize_t DeviceFile::pread(char *buf, size_t len, off_t offset)
{
	return ::pread(m_fd, buf, len, offset);
}

off_t DeviceFile::size()
{
	struct stat st;
	memset(&st, 0, sizeof(st));
	fstat(m_fd, &st);
	return st.st_size;
}

