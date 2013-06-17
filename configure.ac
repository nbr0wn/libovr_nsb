m4_define([OVR_VERSION_STRING], [0.3.0])
m4_define([OVR_NAME], [libovr_nsb])
m4_define([OVR_EMAIL], [nbrown1@gmail.com])

AC_PREREQ([2.13])
AC_INIT([OVR_NAME], [OVR_VERSION_STRING], [OVR_EMAIL])
AM_INIT_AUTOMAKE([1.10 no-define foreign -Wall -Werror])
AM_PROG_AR
LT_INIT
AC_CANONICAL_HOST

# Silent mode
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_SUBST(PKG_CONFIG_EXTRA_PATH, "")
AC_SUBST(EXTRA_LD_FLAGS, "")

AC_MSG_CHECKING([operating system])

# Check for HIDAPI
PKG_CHECK_MODULES([hidapi], [hidapi-libusb] >= 0.0.5)

# Check for freeglut
AC_CHECK_LIB([glut])

AC_PROG_CC
#AC_PROG_CC_C99

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_OUTPUT([Makefile libovr_nsb/Makefile gl_matrix/Makefile examples/Makefile])
AC_OUTPUT 
