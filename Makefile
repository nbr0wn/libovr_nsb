all:
	mkdir -p m4
	./autogen.sh
	./configure
	make

distclean: realclean
clean: realclean

realclean:
	@rm -rf	\
		aclocal.m4 \
		ar-lib \
		autom4te.cache \
		config.guess \
		config.h* \
		config.log \
		config.status \
		config.sub \
		configure \
		depcomp \
		install-sh \
		libtool \
		ltmain.sh \
		m4 \
		Makefile.in \
		missing \
		stamp-h1 \
		examples/Makefile.in \
		examples/Makefile \
		examples/.deps \
		examples/*.o \
		examples/consoletest \
		examples/gldemo \
		examples/hmd_orientation \
		examples/.libs \
		libovr_nsb/*.o \
		libovr_nsb/*.la \
		libovr_nsb/*.lo \
		libovr_nsb/Makefile.in \
		libovr_nsb/Makefile.in \
		libovr_nsb/Makefile \
		libovr_nsb/.deps \
		libovr_nsb/.libs \
		gl_matrix/*.o \
		gl_matrix/*.la \
		gl_matrix/*.lo \
		gl_matrix/Makefile.in \
		gl_matrix/Makefile \
		gl_matrix/.deps \
		gl_matrix/.libs \
		packages/libovr*
	@cp -f Makefile.clean Makefile
