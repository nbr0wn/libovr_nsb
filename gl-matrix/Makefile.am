library_includedir=$(top_builddir)/gl-matrix
glmatrixdir = $(includedir)/gl-matrix
glmatrix_HEADERS=gl-matrix.h

lib_LTLIBRARIES = libgl-matrix.la
libgl-matrix_la_SOURCES = \
						mat3.c \
						mat4.c \
						quat.c \
						str.c \
						vec3.c

libgl-matrix_la_LDFLAGS = -no-undefined -release 1.2.3 $(EXTRA_LD_FLAGS) -lm
libgl-matrix_la_CPPFLAGS = -fPIC -Wall -Werror
