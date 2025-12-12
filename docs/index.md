# MultipackParser Documentation

Welcome to the MultipackParser documentation! This application is designed to parse and manage data from Multipack Robots and communicate with Universal Robots (UR) robotic systems.

## Overview

MultipackParser is a comprehensive industrial automation tool that bridges the gap between Multipack Robot systems and Universal Robots (UR10/UR20). It provides a graphical user interface for managing pallet configurations, monitoring robot status, and facilitating seamless data transfer between systems.

![Main Interface](assets/MultipackParser-MainUI.png)

*Main interface of MultipackParser after startup*

### Key Features

- **Pallet Management**: Load, visualize, and manage pallet configurations with 3D visualization
- **Robot Communication**: Real-time communication with UR robots via XML-RPC server
- **Database Integration**: SQLite database for storing pallet data and configurations
- **Status Monitoring**: Real-time monitoring of robot status, safety systems, and scanner states
- **Multi-Robot Support**: Supports both UR10 and UR20 robot models
- **Security**: Password-protected settings and operations
- **Audio Feedback**: Configurable audio notifications for system events

## Documentation Structure

### [Installation Guide](installation.md)
Complete setup instructions for installing and configuring MultipackParser on your system.

### [User Guide](user-guide.md)
Comprehensive user manual covering all features, workflows, and common tasks.

### [Architecture Documentation](architecture.md)
Technical documentation for developers, including system architecture, module structure, and design patterns.

### [API Reference](api-reference.md)
Complete reference for the XML-RPC API functions available for robot communication.

## Quick Start

1. **Install Dependencies**
   ```bash
   pip install -r requirements.txt
   ```

2. **Run the Application**
   ```bash
   python main.py
   ```

3. **Check Version**
   ```bash
   python main.py --version
   ```

## System Requirements

- **Python**: >= 3.13
- **Operating System**: Linux (optimized for Raspberry Pi), Windows
- **Dependencies**: See `requirements.txt` for complete list
- **Hardware**: Compatible with Universal Robots UR10 and UR20

> [!IMPORTANT]
> This application requires robot-side components that are **not included** in this repository:
> - A specific **UR Program** must be installed and running on the Universal Robot
> - **URscript** code is required on the robot to interact with the XML-RPC server
> - These robot-side components must be obtained separately and configured on your Universal Robot controller

## Getting Help

- Check the [User Guide](user-guide.md) for common questions
- Review the [Architecture Documentation](architecture.md) for development questions
- Examine the [API Reference](api-reference.md) for integration details

## License

This project is licensed under the GNU General Public License v3.0. See the [LICENSE](../LICENSE) file for details.

