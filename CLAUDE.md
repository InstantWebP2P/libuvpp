# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is **libuvpp**, a fork of libuv with minimal changes for P2P networking. It extends the original libuv with additional transport protocols:

- **UDT (UDP-based Data Transfer)** - High-performance data transfer protocol
- **KCP** - Fast and reliable ARQ protocol
- **QUIC** - Modern transport protocol over UDP

This is part of a dual-component project with **nodejs-httpp** that enables HTTP over UDP-based transports.

## Build System

### CMake Build (Recommended)

```bash
mkdir -p build
(cd build && cmake .. -DBUILD_TESTING=ON)
cmake --build build
```

### Autotools Build (Unix-like systems)

```bash
sh autogen.sh
./configure
make
make check
make install
```

### Testing

Run all tests:
```bash
(cd build && ctest -C Debug --output-on-failure)
```

Run specific core libuv test:
```bash
build/uv_run_tests_a TEST_NAME
```

Run benchmarks:
```bash
build/uv_run_benchmarks_a
```

Run UDT protocol tests:
```bash
build/echo-server-udt &
build/echo-client-udt
```

Run KCP protocol tests:
```bash
build/echo-server-kcp &
build/echo-client-kcp
```

Run additional KCP test executables:
```bash
build/test-kcp-simple
build/test-server-client
```

For timing-sensitive tests on slow machines:
```bash
env UV_TEST_TIMEOUT_MULTIPLIER=2 build/uv_run_tests_a
```

### Build Options

- `-DLIBUV_BUILD_SHARED=ON/OFF` - Build shared library (default: ON)
- `-DASAN=ON` - Enable AddressSanitizer
- `-DTSAN=ON` - Enable ThreadSanitizer
- `-DUBSAN=ON` - Enable UndefinedBehaviorSanitizer
- `-DQEMU=ON` - Build for QEMU

### Submodule Initialization

Protocol extensions are git submodules. Initialize them before building:
```bash
git submodule update --init --recursive
```

**Note**: Current git status shows submodules have modifications. Check submodule status with:
```bash
git status -b .
```

### Quick Build Verification

After building, verify protocol extensions are included:
```bash
nm build/libuv.a | grep -i udt
nm build/libuv.a | grep -i kcp
```

## Architecture

### Core Components

- **src/** - Core libuv implementation
  - `src/unix/` - Unix-specific implementations
  - `src/win/` - Windows-specific implementations
- **include/** - Public API headers
  - `uv.h` - Main API header
- **test/** - Test suite and benchmarks

### Extended Protocols

- **uvudt/** - UDT transport integration
  - `uvudt/UDT4/` - UDT v4 library
  - `uvudt/uvudt.c` - libuv UDT wrapper
  - `uvudt/udtstream.c` - UDT stream implementation

- **uvkcp/** - KCP transport integration
  - `uvkcp/kcp/` - KCP library
  - `uvkcp/uvkcp.c` - libuv KCP wrapper
  - `uvkcp/kcpstream.c` - KCP stream implementation

- **uvquic/** - QUIC transport integration
  - `uvquic/quic++.cpp` - QUIC++ implementation (currently empty)
  - `uvquic/uvquic.c` - libuv QUIC wrapper (currently empty)
  - `uvquic/quicstream.c` - QUIC stream implementation (currently empty)

### Platform Support

- **Windows**: Uses CMake only, requires Visual Studio Build Tools
- **Unix-like**: Supports both CMake and autotools
- **Cross-compilation**: Supported via CMake

## Development Notes

### Code Conventions

- C code follows C90 standard with GNU extensions
- C++ code follows C++11 standard
- Platform-specific code separated into `src/unix/` and `src/win/`
- Public API defined in `include/uv.h`

### Testing

- Test list defined in `test/test-list.h`
- Benchmarks defined in `test/benchmark-list.h`
- Tests are timing-sensitive, use `UV_TEST_TIMEOUT_MULTIPLIER=2` on slow machines

### Protocol Extensions

Each protocol extension follows a similar pattern:
- Protocol wrapper file (e.g., `uvudt.c`)
- Stream implementation (e.g., `udtstream.c`)
- Protocol library integration
- Test executables in protocol-specific test directories

**Key Implementation Files**:
- UDT: `uvudt/uvudt.c`, `uvudt/udtstream.c`, `uvudt/UDT4/src/*.cpp`
- KCP: `uvkcp/uvkcp.c`, `uvkcp/kcpstream.c`, `uvkcp/kcp/ikcp.c`
- QUIC: Files exist but are currently empty

### Important Build Flags

- `-fno-strict-aliasing` recommended for projects using libuv
- Compiler-specific warnings and optimizations configured in CMakeLists.txt

## Common Development Tasks

### Adding New Tests

1. Add test file to `test/` directory
2. Update `test/test-list.h` with test declaration
3. Test will be automatically included in CMake build

### Building Protocol Extensions

Protocol extensions are automatically included in the main build. Each extension adds:
- Source files to `uv_sources` list in CMakeLists.txt (lines 181-202)
- Include directories for protocol headers
- Test executables (e.g., `echo-server-udt`, `echo-server-kcp`)

**Build Integration**: All protocol sources are compiled directly into the main libuv library

### Debugging

Use fork-aware debugging tools:
- GDB: `set follow-fork-mode child`
- Valgrind: `--trace-children=yes`

## Important Notes

### Protocol Extension Status
- **UDT**: Fully integrated with UDT4 library and test executables (`echo-server-udt`, `echo-client-udt`)
- **KCP**: Fully integrated with KCP library and test executables (`echo-server-kcp`, `echo-client-kcp`, `test-kcp-simple`, `test-server-client`)
- **QUIC**: Basic structure in place but implementation files are currently empty

### Build Integration
- Protocol extensions are compiled directly into the main libuv library
- All protocol sources are included in `uv_sources` list in CMakeLists.txt
- Protocol-specific headers are added to include directories
- UDT4 library is compiled from source as part of the build

### Testing Protocol Extensions
- UDT has dedicated test executables: `echo-server-udt` and `echo-client-udt`
- KCP has dedicated test executables: `echo-server-kcp`, `echo-client-kcp`, `test-kcp-simple`, and `test-server-client`
- Protocol tests follow the same pattern as core libuv tests

### Build Verification

After building, verify the library includes protocol extensions:
```bash
nm build/libuv.a | grep -i udt
nm build/libuv.a | grep -i kcp
```

**Note**: KCP test executables are now built by default in CMakeLists.txt (lines 768-791). Recent commits have added support for building KCP test executables.

## Current Development State

### Recent Changes
- **KCP Integration**: Recent commits (7beff81f) have fully integrated KCP protocol with test executables
- **Build System**: CMakeLists.txt updated to include KCP test executables by default
- **Submodules**: uvkcp and uvudt submodules have recent updates with compilation fixes

### Active Development Branch
- Current branch: `v1.x-udt`
- Branch is ahead of origin by 5 commits
- Protocol extensions are actively being developed and tested