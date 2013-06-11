bin_PROGRAMS = consoletest hmd_orientation gldemo
AM_CFLAGS = $(hidapi_CFLAGS) -fPIC -I$(top_srcdir)/libovr_nsb/lib -I$(top_srcdir)/libovr_nsb/gl-matrix.c
AM_LDFLAGS = $(hidapi_LIBS) $(EXTRA_LD_FLAGS) $(top_srcdir)/libovr_nsb/lib/libovr_nsb.la $(top_srcdir)/libovr_nsb/gl-matrix.c/libgl_matrix_c.la -lpthread -lglut -lGL -lGLU -lhidapi-libusb -lm
consoletest_SOURCES = consoletest.c
hmd_orientation_SOURCES = hmd_orientation.c
gldemo_SOURCES = gldemo.c
