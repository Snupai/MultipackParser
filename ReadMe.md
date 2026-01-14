# MultipackParser C++ Version

C++ Qt6 rewrite of the MultipackParser application for enhanced performance on Raspberry Pi.

## Overview

MultipackParser is a bridge application connecting Multipack palette optimization software with Universal Robots (UR10/UR20) palletizing systems. This C++ version provides:

- Improved performance on ARM64 (Raspberry Pi)
- Native Qt6 widgets
- VTK-based 3D visualization
- Reduced memory footprint

## Prerequisites

### Required Dependencies

- **Qt6** (6.2+ recommended): Core, Widgets, Network, Sql, Multimedia
- **CMake** 3.16+
- **C++17** compatible compiler
- **SQLite3**

### Optional Dependencies

- **VTK 9.x**: For 3D visualization
- **xmlrpc-c**: For XML-RPC server
- **OpenSSL**: For password encryption

### Ubuntu/Debian Installation

```bash
# Qt6 and build tools
sudo apt install qt6-base-dev qt6-multimedia-dev qt6-tools-dev cmake build-essential

# SQLite
sudo apt install libsqlite3-dev

# Optional: VTK
sudo apt install libvtk9-qt-dev

# Optional: xmlrpc-c
sudo apt install libxmlrpc-c++8-dev
```

### Raspberry Pi OS

```bash
# Enable Qt6 repository if needed
sudo apt update
sudo apt install qt6-base-dev libsqlite3-dev cmake build-essential
```

## Building

### Windows: Docker Build for Raspberry Pi (Recommended)

The easiest way to build for Raspberry Pi from Windows is using Docker:

```batch
REM First time setup (installs QEMU for ARM64 emulation)
build.bat --setup

REM Build for ARM64 (Raspberry Pi)
build.bat --arm64

REM Build for x86_64 Linux (testing)
build.bat --native

REM Show all options
build.bat --help

REM Clean build artifacts
build.bat --clean
```

The output binary will be in `output/multipack-parser`.

**Requirements:**
- [Docker Desktop](https://www.docker.com/products/docker-desktop/) installed and running
- WSL2 backend enabled (for ARM64 emulation)

### Native Build (Development)

```bash
cd multipack-cpp
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### ARM64 Cross-Compilation (Raspberry Pi)

```bash
cd multipack-cpp
mkdir build-arm64 && cd build-arm64
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Arm64Toolchain.cmake ..
make -j$(nproc)
```

### Using Build Scripts

```bash
# Native build
./scripts/build.sh

# ARM64 build
./scripts/build_arm64.sh
```

## Running

```bash
# Standard run
./build/bin/multipack-parser

# With debug logging
./build/bin/multipack-parser --verbose

# Show version
./build/bin/multipack-parser --version

# Show help
./build/bin/multipack-parser --help
```

### Command Line Options

| Option | Description |
|--------|-------------|
| `--version`, `-V` | Display version information |
| `--verbose`, `-v` | Enable debug logging |
| `--license` | Show license information |
| `--no-virtual-keyboard` | Disable on-screen keyboard |

## Project Structure

```
multipack-cpp/
├── CMakeLists.txt          # Main build configuration
├── src/                    # Source files (.cpp)
│   ├── main.cpp           # Entry point
│   ├── core/              # Application core
│   ├── ui/                # User interface
│   ├── database/          # SQLite database
│   ├── network/           # XML-RPC server
│   ├── robot/             # Robot communication
│   ├── audio/             # Audio playback
│   ├── message/           # Messaging system
│   ├── config/            # Configuration
│   ├── system/            # System utilities
│   └── utils/             # Helper utilities
├── include/multipack/     # Header files (.h)
├── ui/                    # Qt Designer UI files
├── resources/             # Qt resources (icons, audio)
├── cmake/                 # CMake modules
├── docs/                  # Documentation
└── scripts/               # Build scripts
```

## Configuration

Settings are stored in `settings.json`:

```json
{
  "info": {
    "version": "1.7.9",
    "UR_Model": "UR10"
  },
  "robot": {
    "ip": "192.168.0.1"
  },
  "server": {
    "port": 8080
  }
}
```

## Development Status

This is a C++ skeleton with stub implementations. The following components are stubbed:

- [x] Core application framework
- [x] Settings management
- [x] Logging configuration
- [x] Database models
- [x] XML-RPC server structure
- [x] Robot communication framework
- [x] Audio playback system
- [x] Message/status system
- [ ] Full MainWindow implementation
- [ ] 3D visualization (VTK)
- [ ] Complete database operations
- [ ] Full XML-RPC method implementations

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Building](docs/BUILDING.md)
- [Migration Guide](docs/MIGRATION.md)

## License

Proprietary - Szaidel Cosmetic GmbH
