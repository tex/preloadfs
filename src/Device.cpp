#include "Device.hpp"
#include "DeviceFile.hpp"
#include "DeviceHttp.hpp"
#include <string.h>

Device *Device::deviceFactory(const char *name)
{
	if (strncasecmp(name, "http://", 7) == 0)
		return new DeviceHttp();
	else
		return new DeviceFile();
}


