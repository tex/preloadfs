bin_PROGRAMS = preloadfs

common = \
	PreLoadFs.cpp \
	CBuffer.cpp \
	FBuffer.cpp \
	MBuffer.cpp \
	Device.cpp \
	DeviceFile.cpp \
	DeviceHttp.cpp

noinst_HEADERS = \
	PreLoadFs.hpp \
	CBuffer.hpp \
	FBuffer.hpp \
	MBuffer.hpp \
	Device.hpp \
	DeviceFile.hpp \
	DeviceHttp.hpp

preloadfs_SOURCES = $(common) main.cpp
preloadfs_LDADD = $(BOOST_SYSTEM_LIB) $(BOOST_PROGRAM_OPTIONS_LIB) $(FUSE_LIBS)

AM_CXXFLAGS = $(BOOST_CXXFLAGS)

AM_LDFLAGS=$(BOOST_LDFLAGS)

AM_CPPFLAGS = \
	-D_GNU_SOURCE \
	-D_REENTRANT \
	-D_POSIX_C_SOURCE=200112L \
	-D_POSIX_SOURCE \
	-D_SVID_SOURCE \
	-D_BSD_SOURCE \
	-D_XOPEN_SOURCE=500 \
	-pedantic -Wno-long-long \
	-Wall

