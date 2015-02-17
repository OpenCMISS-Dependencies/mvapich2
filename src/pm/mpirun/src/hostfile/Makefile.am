AM_CPPFLAGS = -I$(top_srcdir)/../../mpid/ch3/channels/common/include
AM_CPPFLAGS += -I$(top_srcdir) -I$(srcdir)/../
AM_YFLAGS = -d -p hostfile_yy

noinst_LIBRARIES = libhostfile.a

libhostfile_a_SOURCES = parser.y scanner.l
BUILT_SOURCES = parser.h
