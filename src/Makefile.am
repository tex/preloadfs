bin_PROGRAMS = preloadfs

common = \
	PreLoadFs.cpp \
	CBuffer.cpp \
	FBuffer.cpp

noinst_HEADERS = \
	PreLoadFs.hpp \
	CBuffer.hpp \
	FBuffer.hpp

preloadfs_SOURCES = $(common) main.cpp

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

