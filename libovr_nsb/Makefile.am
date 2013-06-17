library_includedir=$(top_builddir)/gl_matrix
libnsbdir = $(includedir)/libovr_nsb
libnsb_HEADERS = \
				 OVR_Defs.h \
				 OVR_Device.h \
				 OVR.h \
				 OVR_HID.h \
				 OVR_Sensor.h

lib_LTLIBRARIES = libovr_nsb.la
libovr_nsb_la_SOURCES = \
						OVR_Helpers.c \
						OVR_HID_hidapi.c \
						OVR_Sensor.c

libovr_nsb_la_LDFLAGS = $(hidapi_LIBS) -no-undefined -release 0.3.0 $(EXTRA_LD_FLAGS) -lm
libovr_nsb_la_CPPFLAGS = -fPIC -I$(top_srcdir) $(hidapi_CFLAGS) -Wall -Werror
