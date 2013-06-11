libovr_nsb v0.2.0
=================

This is a Pure C implementation of the [Oculus Rift](http://oculusvr.com) SDK in static and shared library form.  This library was created to provide access to the drivers and sensor fusion algorithms produced by Oculus VR from basic C functions.

The library uses the excellent hidapi library for the HID heavy lifting.  I had originally created some HID routines myself, but hidapi offers cross-platform support so I decided to switch in the eventual hopes that I'd support other platforms in the future.

This library also uses [gl-matrix.c](https://github.com/Coreh/gl-matrix.c), which is a permissively-licensed vector/matrix/quat library. This choice was pretty arbitrary.  I didn't want to write one myself and I wanted one with a license that was compatible with the Oculus VR SDK.  Only a handful of functions are actually used for the tracker updates, but having a whole library available is handy.

Prerequesites
--------------
+ GLUT (for the examples)
+ HIDAPI
    - [http://www.signal11.us/oss/hidapi/](http://www.signal11.us/oss/hidapi/)


Compiling
---------
There is a makefile included which performs the following actions:

    ./autoconf
    ./configure
    make

You can run these separately if you need to provide other arguments such as the location of the hidapi library or headers.

Excellent idea Shamelessly copied from the [OpenHMD](https://github.com/OpenHMD/OpenHMD) folks:

    Configuring udev on Linux

    To avoid having to run your applications as root to access USB devices you have to 
    add a udev rule (this will be included in .deb packages, etc).

    As root, run:

    echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="2833", MODE="0666", GROUP="plugdev"' > /etc/udev/rules.d/83-hmd.rules
    udevadm control --reload-rules

    After this you have to unplug your Rift and plug it back in. You should now be 
    able to access the Oculus Rift as a normal user.


Installing
----------

    make install

This will install into $PREFIX.  You'll need to export LD_LIBRARY_PATH to point at $PREFIX/lib in order to run the examples

Uninstall
----------

    make uninstall


License
-------
This library falls under the Oculus Rift SDK License.  See License.txt for details

Oculus Rift SDK
----------
(C) Oculus VR, Inc. 2013. All Rights Reserved.
The latest version of the Oculus SDK is available at http://developer.oculusvr.com.


Other HMD Libraries
-------------------

### OpenHMD ###
 + [https://github.com/OpenHMD/OpenHMD](https://github.com/OpenHMD/OpenHMD)

### LibVR ###
 + [http://hg.sitedethib.com/libvr](http://hg.sitedethib.com/libvr)

Contact
--------
If you're on the oculus rift forums ( https://developer.oculusvr.com/forums/ )
you can pm to 'nsb' there.  Otherwise, feel free to email me: nbrown1@gmail.com

Contributors
------------
To Be Announced if they want to be called out

