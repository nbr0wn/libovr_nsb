library_includedir=$(top_builddir)/gl_matrix
glmatrixdir = $(includedir)/gl_matrix
glmatrix_HEADERS=gl_matrix.h

lib_LTLIBRARIES = libgl_matrix.la
libgl_matrix_la_SOURCES = \
						mat3.c \
						mat4.c \
						quat.c \
						str.c \
						vec3.c

libgl_matrix_la_LDFLAGS = -no-undefined -release 1.2.3 $(EXTRA_LD_FLAGS) -lm
libgl_matrix_la_CPPFLAGS = -fPIC -Wall -Werror
