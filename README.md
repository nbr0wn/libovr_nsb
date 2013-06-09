# libovr_nsb v0.2.0#

This is a Pure C implementation of the Oculus Rift SDK.  

I like Linux.  I'll take OSX in a pinch, but I do nearly all of my work under
Linux, so I was a little disappointed to see that the initial Rift support was 
Win32 only.  I'm also impatient, so I decided to see if I could port the win32
stuff to Linux while I wait for better support from Oculus.  

As of today, Oculus has released SDK v2.2 which includes a little more OSX 
support, so that's a step in the right direction, but still no Linux.

The library uses the excellent hidapi library for the HID heavy lifting.  I had
originally created a simple C version myself, but hidapi offers cross-platform 
support so I decided to switch in the eventual hopes that I'd support other 
platforms in the future.

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

Installing
----------
make install

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


Extras:
-------
gl-matrix.c is a vector/matrix/quat library from 
https://github.com/Coreh/gl-matrix.c . This choice was pretty arbitrary.
I didn't want to write one myself and I wanted one with a permissive 
license.  Only a handful of functions are actually used for the tracker 
updates, but having a whole library available is handy, so here it is.

Contact:
--------
If you're on the oculus rift forums ( https://developer.oculusvr.com/forums/ )
you can pm to 'nsb' there.  Otherwise, feel free to email me: nbrown1@gmail.com
