# libuvpp 
![C/C++ CI](https://github.com/InstantWebP2P/libuvpp/workflows/C/C++%20CI/badge.svg)


Porting UDT(UDP-based transport) to libuv as the transport of HTTPP(run http over udp).	
All api is similar to tcp. it's simple to use it: 	
	1. replace tcp litera with udt, like uv_tcp_t to uv_udt_t.	
	2. do the rest as tcp.

To build it manually, clone the repository and checkout v0.8-udt branch firstly, then do steps as below.

Third-party source:
[UDT4](http://udt.sourceforge.net)

[Discussion group](https://groups.google.com/d/forum/iwebpp)

[Wiki Page](https://github.com/InstantWebP2P/libuvpp/wiki/An-introduction-to-libuvpp)

## Features

 * Non-blocking TCP sockets

 * Non-blocking named pipes

 * UDP
 
 * UDT transport sockets

 * Timers

 * Child process spawning

 * Asynchronous DNS via c-ares or `uv_getaddrinfo`.

 * Asynchronous file system APIs `uv_fs_*`

 * High resolution time `uv_hrtime`

 * Current executable path look up `uv_exepath`

 * Thread pool scheduling `uv_queue_work`

 * ANSI escape code controlled TTY `uv_tty_t`

 * File system events Currently supports inotify, `ReadDirectoryChangesW`
   and kqueue. Event ports in the near future.
   `uv_fs_event_t`

 * IPC and socket sharing between processes `uv_write2`


## Documentation

See `include/uv.h`.


## Build Instructions

For GCC (including MinGW) there are two methods building: via normal
makefiles or via GYP. GYP is a meta-build system which can generate MSVS,
Makefile, and XCode backends. It is best used for integration into other
projects.  The old (more stable) system is using Makefiles.

To checkout the sourcecode:

    git clone https://github.com/InstantWebP2P/libuvpp.git
    git checkout v0.8-udt


To build via Makefile simply execute:

    make

To build with Visual Studio run the vcbuilds.bat file which will
checkout the GYP code into build/gyp and generate the uv.sln and
related files.

Windows users can also build from cmd-line using msbuild.  This is
done by running vcbuild.bat from Visual Studio command prompt.

To have GYP generate build script for another system you will need to
checkout GYP into the project tree manually:

    svn co http://gyp.googlecode.com/svn/trunk build/gyp

Unix users run

    ./gyp_uv -f make
    make -C out

Macintosh users run

    ./gyp_uv -f xcode
    xcodebuild -project uv.xcodeproj -configuration Release -target All

Android users run. notes: please MUST not build with BUILDTYPE=Release

    $ source ./android-configure NDK_PATH gyp
    $ make -C out or make uv -C out


## Supported Platforms

Microsoft Windows operating systems since Windows XP SP2. It can be built
with either Visual Studio or MinGW.

Linux 2.6 using the GCC toolchain.

MacOS using the GCC or XCode toolchain.

Solaris 121 and later using GCC toolchain.
