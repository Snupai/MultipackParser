# Building MultipackParser C++

## Prerequisites

### Development Machine (Ubuntu/Debian)

```bash
# Essential build tools
sudo apt install build-essential cmake pkg-config

# Qt6 development packages
sudo apt install qt6-base-dev qt6-tools-dev qt6-multimedia-dev

# SQLite3
sudo apt install libsqlite3-dev

# Optional: VTK for 3D visualization
sudo apt install libvtk9-qt-dev

# Optional: xmlrpc-c for XML-RPC server
sudo apt install libxmlrpc-c++8-dev

# Optional: OpenSSL for encryption
sudo apt install libssl-dev
```

### Cross-Compilation Tools (for ARM64)

```bash
# ARM64 cross compiler
sudo apt install g++-aarch64-linux-gnu

# ARM64 Qt6 libraries (may need to build from source)
# See: https://wiki.qt.io/Cross-Compile_Qt_6_for_Raspberry_Pi
```

### Raspberry Pi OS

```bash
# Update system
sudo apt update && sudo apt upgrade

# Install Qt6 (may need to enable additional repositories)
sudo apt install qt6-base-dev qt6-multimedia-dev

# SQLite
sudo apt install libsqlite3-dev

# Build tools
sudo apt install cmake build-essential
```

## Build Types

### Debug Build

```bash
mkdir build-debug && cd build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

### Release Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Release with Debug Info

```bash
mkdir build-relwithdebinfo && cd build-relwithdebinfo
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
make -j$(nproc)
```

## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Release | Build type (Debug/Release/RelWithDebInfo) |
| `ENABLE_VTK` | ON | Enable VTK 3D visualization |
| `ENABLE_XMLRPC` | ON | Enable XML-RPC server |
| `WARNINGS_AS_ERRORS` | OFF | Treat warnings as errors |

Example:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_VTK=OFF
```

## Build Scripts

### Native Build

```bash
./scripts/build.sh
```

### ARM64 Cross-Compilation

```bash
./scripts/build_arm64.sh
```

## Docker Build

For consistent builds, use Docker:

```bash
# Build Docker image
docker build -t multipack-cpp-builder .

# Run build
docker run -v $(pwd):/src multipack-cpp-builder
```

## Troubleshooting

### Qt6 Not Found

```
CMake Error: Qt6 not found
```

**Solution:** Set Qt6 path:

```bash
export Qt6_DIR=/path/to/qt6/lib/cmake/Qt6
cmake ..
```

### VTK Not Found

```
CMake Warning: VTK not found, 3D visualization disabled
```

**Solution:** Install VTK or disable:

```bash
cmake .. -DENABLE_VTK=OFF
```

### ARM64 Toolchain Issues

```
Error: aarch64-linux-gnu-g++ not found
```

**Solution:**

```bash
sudo apt install g++-aarch64-linux-gnu
```

### Linker Errors

```
undefined reference to `sqlite3_open`
```

**Solution:** Ensure SQLite is installed and linked:

```bash
sudo apt install libsqlite3-dev
```

## Output

After successful build:

```
build/
├── bin/
│   └── multipack-parser    # Main executable
└── lib/
    └── *.so                # Shared libraries (if any)
```

## Running Tests

```bash
cd build
ctest --output-on-failure
```

## Installing

```bash
cd build
sudo make install
```

Default installation prefix: `/usr/local`

Custom prefix:

```bash
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/multipack
make install
```
