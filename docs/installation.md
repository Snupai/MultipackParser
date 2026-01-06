# Installation Guide

This guide provides comprehensive instructions for installing and configuring MultipackParser on your system.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Installation Methods](#installation-methods)
3. [Configuration](#configuration)
4. [Verification](#verification)
5. [Troubleshooting](#troubleshooting)
6. [Updating](#updating)
7. [Uninstallation](#uninstallation)

---

## Prerequisites

### System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| **Python** | 3.13 | 3.13+ |
| **RAM** | 512 MB | 1 GB |
| **Storage** | 100 MB | 200 MB |
| **OS** | Linux / Windows 10 | Raspberry Pi OS / Windows 11 |

### Required Software

| Software | Purpose |
|----------|---------|
| Python 3.13+ | Runtime environment |
| pip | Python package manager |
| Git | Repository cloning (optional) |
| [Multipack](https://multiscience.de/multipack-ihre-optimierungssoftware/) | Palette plan generation (commercial) |

### Robot-Side Requirements

> [!IMPORTANT]
> MultipackParser requires robot-side components that are **not provided** in this repository:
> 
> | Component | Description |
> |-----------|-------------|
> | **UR Program** | A specific Universal Robot program installed on the UR10/UR20 controller |
> | **URscript** | Scripts on the robot controller that interact with the XML-RPC server |
> 
> These components must be obtained separately and properly configured on your robot. Without them, the application cannot communicate with the Universal Robot.

---

## Installation Methods

### Method 1: From Source (Development)

Recommended for developers and users who want the latest features.

```bash
# 1. Clone the repository
git clone https://github.com/Snupai/MultipackParser.git
cd MultipackParser

# 2. Install dependencies
pip install -r requirements.txt

# Or using uv (faster alternative)
uv sync

# 3. Verify installation
python main.py --version

# Or using uv
uv run main.py --version
```

### Method 2: Pre-built Binary

For Raspberry Pi (ARM64) systems, download the pre-built binary:

1. **Download the Latest Release**
   - Visit the [Releases page](https://github.com/Snupai/MultipackParser/releases)
   - Download the binary matching your architecture

2. **Extract and Run**
   ```bash
   chmod +x MultipackParser
   ./MultipackParser
   ```

### Method 3: Docker Build

Build a custom ARM64 binary using Docker:

```bash
# Ensure Docker is installed
docker --version

# Build the application
python build.py
```

The resulting binary will be placed in the `local_dist` directory.

> [!NOTE]
> The Docker build creates an ARM64-compatible binary for Raspberry Pi deployment.

---

## Configuration

### Initial Setup

On first run, the application automatically:
- Creates the `logs/` directory for application logs
- Initializes the SQLite database (`paletten.db`)
- Generates default configuration files

```bash
# First run
python main.py
```

### Settings Configuration

Access settings through the application UI (password-protected):

| Setting Category | Description |
|------------------|-------------|
| **Info** | Robot model selection (UR10/UR20), system information |
| **Network** | Connection parameters |
| **Audio** | Sound notification preferences |
| **Security** | Password management |

### Robot Model Configuration

| Model | Features |
|-------|----------|
| **UR10** | 2 scanners, standard palette support |
| **UR20** | 3 scanners, dual palette support, intermediate layers |

### Environment Variables

The application sets these automatically, but they can be overridden if needed:

| Variable | Default | Purpose |
|----------|---------|---------|
| `QT_X11_NO_MITSHM` | 1 | Disable MIT-SHM extension (Linux) |
| `LIBGL_ALWAYS_SOFTWARE` | 1 | Force software rendering |
| `QT_OPENGL` | software | Use software OpenGL |
| `QT_QPA_PLATFORM` | xcb | Linux display platform |
| `QT_IM_MODULE` | qtvirtualkeyboard | Enable virtual keyboard |

### Database

The application uses SQLite for data storage:
- **Location**: `paletten.db` in the application directory
- **Purpose**: Stores pallet configurations and metadata
- **Management**: Automatic creation and updates

---

## Verification

### Test Installation

```bash
# Check version
python main.py --version
# Expected output: Multipack Parser Application Version: 1.7.9

# Check license
python main.py --license

# Run with verbose logging
python main.py --verbose
```

### Command Line Options

| Option | Short | Description |
|--------|-------|-------------|
| `--version` | `-v` | Display version information |
| `--license` | `-l` | Display license information |
| `--verbose` | `-V` | Enable debug logging |
| `--no-virtual-keyboard` | | Disable virtual keyboard |

### Verify Dependencies

Check that all required packages are installed:

```bash
pip list | grep -E "PySide6|matplotlib|cryptography|pydub|pygame|requests"
```

Expected output should include:
- PySide6 >= 6.7.2
- matplotlib == 3.7.5
- cryptography
- pydub
- pygame
- requests

---

## Troubleshooting

### Common Issues

#### Import Errors

**Symptoms**: `ModuleNotFoundError` when starting the application

**Solution**:
```bash
pip install -r requirements.txt --upgrade
```

#### Qt/OpenGL Errors on Linux

**Symptoms**: OpenGL initialization errors, black screen, or display issues

**Solution**:
```bash
export QT_X11_NO_MITSHM=1
export LIBGL_ALWAYS_SOFTWARE=1
export QT_OPENGL=software
python main.py
```

#### Database Permission Errors

**Symptoms**: Cannot create or write to database file

**Solution**:
```bash
chmod 755 /path/to/MultipackParser
chmod 644 /path/to/MultipackParser/paletten.db
```

#### Virtual Keyboard Not Appearing

**Symptoms**: No on-screen keyboard on touch devices

**Solution**:
```bash
pip install PySide6 --upgrade
# Ensure QT_IM_MODULE is set
export QT_IM_MODULE=qtvirtualkeyboard
```

#### Application Startup Freeze

**Symptoms**: Application hangs during splash screen

**Solution**:
1. Run with verbose logging to identify the issue:
   ```bash
   python main.py --verbose
   ```
2. Check log files in `logs/` directory

### Log Files

Application logs are stored in the `logs/` directory:

| Log File | Purpose |
|----------|---------|
| `multipack_parser_YYYYMMDD_HHMMSS.log` | Application events and errors |
| `server_YYYYMMDD_HHMMSS.log` | XML-RPC server requests and responses |

To enable debug logging:
```bash
python main.py --verbose
```

---

## Updating

### Update from Source

```bash
cd /path/to/MultipackParser
git pull origin main
pip install -r requirements.txt --upgrade
```

### Update Binary

There are two ways to update the MultipackParser binary:

1. **Automatic Update via Application UI**
   
   - Click the hidden button at the top left corner of the main window.
   - Enter the password when prompted.
   - Click the "Search Update" button.
   - The application will automatically check for a new release either on GitHub or on a connected USB drive and prompt you to install if one is found.

2. **Manual Update**
   
   - Download the latest release from the [Releases page](https://github.com/Snupai/MultipackParser/releases)
   - Replace the old binary with the new one
   - Restart the application

### Backup Before Update

It's recommended to backup your data before updating:

```bash
# Backup database
cp paletten.db paletten.db.backup

# Backup settings (if applicable)
cp -r config/ config.backup/
```

---

## Uninstallation

### Remove Source Installation

```bash
cd /path/to
rm -rf MultipackParser
```

### Remove Python Packages (Optional)

If you want to remove the installed Python packages:

```bash
pip uninstall PySide6 matplotlib cryptography pydub pygame requests python-dotenv ffmpeg-python
```

### Remove Binary Installation

Simply delete the application directory and binary file:

```bash
rm -rf /path/to/MultipackParser
```

### Clean Up Data

To remove all application data:

```bash
rm -f paletten.db
rm -rf logs/
```

---

## Next Steps

After successful installation:

1. **[User Guide](user-guide.md)** - Learn how to use the application
2. **[Architecture Documentation](architecture.md)** - Understand the system design
3. **[API Reference](api-reference.md)** - Integrate with robot systems

---

<div align="center">
  <sub>MultipackParser Installation Guide - Version 1.7.9</sub>
</div>
