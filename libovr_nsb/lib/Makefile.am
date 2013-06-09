library_includedir=$(top_builddir)/libovr_nsb/gl-matrix.c
library_include_HEADERS = \
						  $(top_srcdir)/libovr_nsb/gl-matrix.c/gl-matrix.h \
						  $(top_srcdir)/libovr_nsb/lib/OVR_Defs.h \
						  $(top_srcdir)/libovr_nsb/lib/OVR_Device.h \
						  $(top_srcdir)/libovr_nsb/lib/OVR.h \
						  $(top_srcdir)/libovr_nsb/lib/OVR_HID.h \
						  $(top_srcdir)/libovr_nsb/lib/OVR_Sensor.h

lib_LTLIBRARIES = libovr_nsb.la
libovr_nsb_la_SOURCES = \
						OVR_Helpers.c \
						OVR_HID_hidapi.c \
						OVR_Sensor.c

libovr_nsb_la_LDFLAGS = $(hidapi_LIBS) -no-undefined -version-info 0:0:0 $(EXTRA_LD_FLAGS) -lm
libovr_nsb_la_CPPFLAGS = -fPIC -I$(top_srcdir)/libovr_nsb/gl-matrix.c -I$(top_srcdir)/libovr_nsb/lib $(hidapi_CFLAGS) -Wall -Werror
