bin_PROGRAMS = consoletest hmd_orientation gldemo
AM_CFLAGS = $(hidapi_CFLAGS) -fPIC -I$(top_srcdir)
AM_LDFLAGS = $(hidapi_LIBS) $(EXTRA_LD_FLAGS) -L$(top_srcdir)/libovr_nsb/.libs -L$(top_srcdir)/gl_matrix/.libs -lpthread -lglut -lGL -lGLU -lhidapi-libusb -lm -lovr_nsb -lgl_matrix
consoletest_SOURCES = consoletest.c
hmd_orientation_SOURCES = hmd_orientation.c
gldemo_SOURCES = gldemo.c glstereo.c glstereo.h gltools.c gltools.h
