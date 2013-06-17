#!/bin/sh

# Make the package directories
mkdir -p \
    libovrnsb/usr/include/gl_matrix \
    libovrnsb/usr/include/libovr_nsb \
    libovrnsb/DEBIAN \
    libovrnsb/usr/lib 

# Create control file
#cp control_32 libovrnsb/DEBIAN/control
cp control_64 libovrnsb/DEBIAN/control

# Copy over libs
cp ../libovr_nsb/.libs/*.so libovrnsb/usr/lib
cp ../gl_matrix/.libs/*.so libovrnsb/usr/lib

# Copy over includes
cp ../libovr_nsb/*.h libovrnsb/usr/include/libovr_nsb
cp ../gl_matrix/gl_matrix.h libovrnsb/usr/include/gl_matrix


# Build the package file
dpkg --build libovrnsb ./
