library_includedir=$(top_builddir)/libovr_nsb/gl-matrix.c
library_include_HEADERS = gl-matrix.h

lib_LTLIBRARIES = libgl_matrix_c.la
libgl_matrix_c_la_SOURCES = \
						mat3.c \
						mat4.c \
						quat.c \
						str.c \
						vec3.c

libgl_matrix_c_la_LDFLAGS = $(hidapi_LIBS) -no-undefined -version-info 0:0:0 $(EXTRA_LD_FLAGS) -lm
libgl_matrix_c_la_CPPFLAGS = -fPIC $(hidapi_CFLAGS) -Wall -Werror
