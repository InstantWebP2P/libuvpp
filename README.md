<<<<<<< HEAD
# libuv

libuv is a new platform layer for Node. Its purpose is to abstract IOCP on
Windows and epoll/kqueue/event ports/etc. on Unix systems. We intend to
eventually contain all platform differences in this library.
=======
# libuvpp 
![C/C++ CI](https://github.com/InstantWebP2P/libuvpp/workflows/C/C++%20CI/badge.svg)

>>>>>>> origin/v0.8-udt

Porting UDT(UDP-based transport) to libuv as the transport of HTTPP(run http over udp).	
All api is similar to tcp. it's simple to use it:
```
	1. replace tcp litera with udt, like uv_tcp_t to uv_udt_t.	
	2. do the rest as tcp.
```

To build it manually, clone the repository and checkout v0.8-udt branch firstly, then do steps as below.

Third-party source:
[UDT4](http://udt.sourceforge.net)

[Discussion group](https://groups.google.com/d/forum/iwebpp)

[Wiki Page](https://github.com/InstantWebP2P/libuvpp/wiki/An-introduction-to-libuvpp)

**This branch only receives security fixes and will be EOL'd by the end of 2016,
please switch to version v1.x**

## Features

 * Non-blocking TCP sockets

 * Non-blocking named pipes

 * UDP
 
 * UDT transport sockets

 * Timers

 * Child process spawning

 * Asynchronous DNS via `uv_getaddrinfo`.

 * Asynchronous file system APIs `uv_fs_*`

 * High resolution time `uv_hrtime`

 * Current executable path look up `uv_exepath`

 * Thread pool scheduling `uv_queue_work`

 * ANSI escape code controlled TTY `uv_tty_t`

 * File system events Currently supports inotify, `ReadDirectoryChangesW`
   and kqueue. Event ports in the near future.
   `uv_fs_event_t`

 * IPC and socket sharing between processes `uv_write2`

## Community

 * [Mailing list](http://groups.google.com/group/libuv)

## Documentation

 * [include/uv.h](https://github.com/libuv/libuv/blob/master/include/uv.h)
   &mdash; API documentation in the form of detailed header comments.
 * [An Introduction to libuv](http://nikhilm.github.com/uvbook/) &mdash; An
   overview of libuv with tutorials.
 * [LXJS 2012 talk](http://www.youtube.com/watch?v=nGn60vDSxQ4) - High-level
   introductory talk about libuv.
 * [Tests and benchmarks](https://github.com/libuv/libuv/tree/master/test) -
   API specification and usage examples.

## Build Instructions

For GCC (including MinGW) there are two methods building: via normal
makefiles or via GYP. GYP is a meta-build system which can generate MSVS,
Makefile, and XCode backends. It is best used for integration into other
projects.  The old system is using plain GNU Makefiles.

To checkout the sourcecode:

    git clone https://github.com/InstantWebP2P/libuvpp.git
    git checkout v0.8-udt


To build via Makefile simply execute:

    make

MinGW users should run this instead:

    make PLATFORM=mingw

Out-of-tree builds are supported:

    make builddir_name=/path/to/builddir

To build with Visual Studio run the vcbuild.bat file which will
checkout the GYP code into build/gyp and generate the uv.sln and
related files.

Windows users can also build from cmd-line using msbuild.  This is
done by running vcbuild.bat from Visual Studio command prompt.

To have GYP generate build script for another system, make sure that
you have Python 2.6 or 2.7 installed, then checkout GYP into the
project tree manually:

    git clone https://chromium.googlesource.com/external/gyp.git build/gyp

Unix users run

<<<<<<< HEAD
    ./gyp_uv.py -f make
=======
    ./gyp_uv -f make
>>>>>>> origin/v0.8-udt
    make -C out

Macintosh users run

    ./gyp_uv.py -f xcode
    xcodebuild -project uv.xcodeproj -configuration Release -target All

<<<<<<< HEAD
Note for UNIX users: compile your project with `-D_LARGEFILE_SOURCE` and
`-D_FILE_OFFSET_BITS=64`. GYP builds take care of that automatically.

Note for Linux users: compile your project with `-D_GNU_SOURCE` when you
include `uv.h`. GYP builds take care of that automatically. If you use
autotools, add a `AC_GNU_SOURCE` declaration to your `configure.ac`.
=======
Android users run. notes: please MUST not build with BUILDTYPE=Release

    $ source ./android-configure NDK_PATH gyp
    $ make -C out or make uv -C out

>>>>>>> origin/v0.8-udt

## Supported Platforms

Microsoft Windows operating systems since Windows XP SP2. It can be built
with either Visual Studio or MinGW.

Linux 2.6 using the GCC toolchain.

MacOS using the GCC or XCode toolchain.

Solaris 121 and later using GCC toolchain.
