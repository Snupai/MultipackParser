# Installation Guide

This guide will walk you through installing and setting up MultipackParser on your system.

## Prerequisites

### System Requirements

- **Python**: Version 3.13 or higher
- **Operating System**: 
  - Linux (optimized for Raspberry Pi)
  - Windows 10/11
- **Memory**: Minimum 512MB RAM (1GB recommended)
- **Storage**: At least 100MB free disk space

### Required Software

- Python 3.13+
- pip (Python package manager)
- Git (for cloning the repository)

### Robot-Side Requirements

> [!IMPORTANT]
> MultipackParser requires robot-side components that are **not provided** in this repository:
> - **UR Program**: A specific Universal Robot program must be installed and running on your UR10 or UR20 controller
> - **URscript**: Scripts on the robot controller that interact with the XML-RPC server
> - These components must be obtained separately and properly configured on your robot
>
> Without these robot-side components, the application will not be able to communicate with the Universal Robot, even if MultipackParser is correctly installed.

## Installation Methods

### Method 1: From Source (Development)

1. **Clone the Repository**
   ```bash
   git clone https://github.com/Snupai/MultipackParser.git
   cd MultipackParser
   ```

2. **Install Dependencies**
   ```bash
   pip install -r requirements.txt
   ```

   Or using `uv` (if available):
   ```bash
   uv pip install -r requirements.txt
   ```

3. **Verify Installation**
   ```bash
   python main.py --version
   ```

### Method 2: Using Pre-built Binary

For Raspberry Pi (ARM64) systems, you can use the pre-built binary:

1. **Download the Latest Release**
   - Visit the [Releases page](https://github.com/Snupai/MultipackParser/releases)
   - Download the appropriate binary for your architecture

2. **Extract and Run**
   ```bash
   tar -xzf MultipackParser-*.tar.gz
   cd MultipackParser
   ./MultipackParser
   ```

### Method 3: Docker Build

For building a custom binary using Docker:

1. **Ensure Docker is Installed**
   ```bash
   docker --version
   ```

2. **Build the Application**
   ```bash
   python build.py
   ```

   The resulting binary will be placed in the `local_dist` directory.

   > [!NOTE]
   > The Docker build process creates an ARM64-compatible binary.

## Configuration

### Initial Setup

1. **First Run**
   - Launch the application: `python main.py`
   - The application will create necessary directories and configuration files
   - Database will be automatically initialized

2. **Settings Configuration**
   - Access settings through the application UI (password-protected)
   - Configure robot model (UR10 or UR20)
   - Set network parameters if needed

### Environment Variables

The application uses several environment variables for configuration:

- `QT_X11_NO_MITSHM`: Disables MIT-SHM extension (Linux)
- `LIBGL_ALWAYS_SOFTWARE`: Forces software rendering
- `QT_OPENGL`: Set to "software" for software OpenGL
- `QT_QPA_PLATFORM`: Set to "xcb" for Linux

These are automatically set by the application, but can be overridden if needed.

### Database Setup

The application uses SQLite for data storage. The database file (`paletten.db`) is automatically created in the application directory on first run.

## Verification

### Test Installation

1. **Check Version**
   ```bash
   python main.py --version
   ```
   Should display: `MultipackParser Application Version: 1.7.6`

2. **Check License**
   ```bash
   python main.py --license
   ```

3. **Run with Verbose Logging**
   ```bash
   python main.py --verbose
   ```

### Verify Dependencies

Check that all required packages are installed:

```bash
pip list | grep -E "PySide6|matplotlib|cryptography|pydub|pygame|requests"
```

## Troubleshooting

### Common Issues

#### Issue: Import Errors

**Solution**: Ensure all dependencies are installed:
```bash
pip install -r requirements.txt
```

#### Issue: Qt/OpenGL Errors on Linux

**Solution**: The application automatically sets software rendering. If issues persist:
```bash
export QT_X11_NO_MITSHM=1
export LIBGL_ALWAYS_SOFTWARE=1
export QT_OPENGL=software
```

#### Issue: Database Permission Errors

**Solution**: Ensure the application directory is writable:
```bash
chmod 755 /path/to/MultipackParser
```

#### Issue: Virtual Keyboard Not Appearing

**Solution**: Ensure Qt virtual keyboard module is available:
```bash
pip install PySide6 --upgrade
```

### Log Files

Log files are stored in the `logs/` directory:
- Application logs: `multipack_parser_YYYYMMDD_HHMMSS.log`
- Server logs: `server_YYYYMMDD_HHMMSS.log`

Check these files for detailed error information.

## Updating

### Update from Source

```bash
git pull origin main
pip install -r requirements.txt --upgrade
```

### Update Binary

1. Download the latest release
2. Replace the old binary
3. Restart the application

## Uninstallation

### Remove from Source Installation

```bash
cd /path/to/MultipackParser
pip uninstall -r requirements.txt
rm -rf /path/to/MultipackParser
```

### Remove Binary Installation

Simply delete the application directory and binary file.

## Next Steps

After installation, proceed to the [User Guide](user-guide.md) to learn how to use the application.

