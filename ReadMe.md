# MultipackParser C++

C++ Qt6 rewrite of the MultipackParser application - a bridge connecting Multipack palette optimization software with Universal Robots (UR10/UR20) palletizing systems.

## Features

- **Native Performance:** C++17 with Qt6 for optimized performance on ARM64 (Raspberry Pi)
- **XML-RPC Server:** Serves palette data to robot controllers on port 8080
- **Robot Communication:** Dashboard server integration for status monitoring
- **SQLite Database:** Stores and manages palette configurations
- **3D Visualization:** Package position visualization (VTK optional)
- **Safety System:** Scanner monitoring with audio warnings
- **USB Support:** Hot-plug detection for palette file loading

## Quick Start

### Prerequisites

- **Qt6** (6.2+): Core, Widgets, Network, Sql, Multimedia
- **CMake** 3.20+
- **C++17** compatible compiler
- **SQLite3**

### Build for Linux with Docker

#### Linux x86_64 / amd64

```bash
docker build -f Dockerfile -t multipack-parser-linux-amd64 --build-arg CMAKE_BUILD_JOBS=8 .
docker create --name multipack-linux-amd64 multipack-parser-linux-amd64
mkdir -p output/multipack-parser-linux-amd64
docker cp multipack-linux-amd64:/app/build/bin/multipack-parser output/multipack-parser-linux-amd64/
docker rm -f multipack-linux-amd64
```

Output: `output/multipack-parser-linux-amd64/multipack-parser`

#### Linux ARM64 / Raspberry Pi

The recommended way to build for Raspberry Pi from any platform:

```bash
# macOS/Linux
./docker-build.sh

# Windows (equivalent to docker-build.sh)
docker-build.bat
docker-build.bat --portable   # Also create single-file launcher (.run)
# Or: build.bat --arm64
```

Output: `output/multipack-parser-arm64.tar.gz` (includes Qt libraries)

Portable single-file: `output/multipack-parser-arm64-portable.run` (use `docker-build.bat --portable` or GitHub Actions)

### Native Build (Development)

```bash
# macOS/Linux
./build.sh
./build.sh --debug --run
./build.sh --portable

# Windows
build.bat
build.bat --debug --run
```

### Manual Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./bin/multipack-parser
```

## Command Line Options

| Option | Description |
|--------|-------------|
| `--version`, `-V` | Display version information |
| `--verbose`, `-v` | Enable debug logging |
| `--license` | Show license information |
| `--no-virtual-keyboard` | Disable on-screen keyboard |

Build script option:
- `./build.sh --portable` creates a single-file Linux launcher (`build/bin/multipack-parser-portable-<arch>.run`)

## Project Structure

```
MultipackParser/
├── CMakeLists.txt              # Build configuration
├── build.sh / build.bat        # Native build scripts
├── Dockerfile                  # Linux x86_64 Docker build
├── docker-build.sh             # ARM64 cross-compilation (macOS/Linux)
├── docker-build.bat            # ARM64 cross-compilation (Windows)
├── Dockerfile.arm64            # ARM64 build container
├── src/                        # Source files
│   ├── main.cpp                # Entry point
│   ├── core/                   # Application core & state
│   ├── ui/                     # User interface
│   ├── database/               # SQLite operations
│   ├── network/                # XML-RPC server
│   ├── robot/                  # Robot communication
│   ├── audio/                  # Audio playback & safety
│   ├── message/                # Status messaging
│   ├── config/                 # Settings management
│   ├── system/                 # USB, updates, file ops
│   └── utils/                  # String/socket utilities
├── include/multipack/          # Header files
├── ui/                         # Qt Designer UI files
└── resources/                  # Icons, audio files
```

## Configuration

Settings stored in `settings.json`:

```json
{
  "info": {
    "version": "2.0.0-beta",
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

## Dependencies Installation

### Ubuntu/Debian

```bash
sudo apt install qt6-base-dev qt6-multimedia-dev qt6-tools-dev \
                 cmake build-essential libsqlite3-dev

# Optional: VTK for 3D visualization
sudo apt install libvtk9-qt-dev
```

### macOS (Homebrew)

```bash
brew install qt@6 cmake sqlite
```

### Windows

Install Qt6 from [qt.io](https://www.qt.io/download) and add to PATH.

## Robot Communication

The application communicates with Universal Robots via:

- **XML-RPC Server (port 8080):** Serves palette data, package positions, layer info
- **Dashboard Server (port 29999):** Monitors robot status, program state, safety

### XML-RPC Boot Contract

On HMI boot the XML-RPC server auto-starts on `0.0.0.0:8080`. The robot should
connect to `http://192.168.0.10:8080/RPC2` on the eth0 robot subnet. The eth0
address is only a reachability target; it is not an application readiness signal.

Robots must poll `get_status()` or `UR_GetStatus()` and treat operational RPCs
as valid only when the returned `ready` field is `true`. Before a palette is
loaded, operational RPCs return an XML-RPC fault instead of default or partial
palette data. Setup/status methods such as `UR_SetFileName`,
`UR_ReadDataFromUsbStick`, `get_status`, and `get_available_functions` remain
available before readiness.

### Supported Robots

| Model | Features |
|-------|----------|
| UR10 | Single palette, basic scanner |
| UR20 | Dual palettes, advanced scanner monitoring |

## Technical Notes

- **Qt Version:** Targets Qt 6.2+ (Ubuntu 22.04 compatible)
- **Thread Safety:** Uses Qt signals/slots for cross-thread UI updates
- **Password Encryption:** SHA256 with random salt (Python-compatible format)
- **Database:** Normalized SQLite schema with cascading foreign keys

## Documentation

- [Building Guide](docs/BUILDING.md)
- [Architecture](docs/architecture.md)
- [Migration from Python](docs/MIGRATION.md)

## License

Proprietary - Szaidel Cosmetic GmbH
