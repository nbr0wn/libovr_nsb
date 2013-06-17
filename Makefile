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
		libovr_nsb/lib/Makefile.in \
		libovr_nsb/lib/Makefile \
		libovr_nsb/lib/.deps \
		libovr_nsb/lib/.libs \
		gl_matrix/Makefile.in \
		gl_matrix/Makefile \
		gl_matrix/.deps \
		gl_matrix/.libs 
	@cp -f Makefile.clean Makefile
