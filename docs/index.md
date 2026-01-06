# MultipackParser Documentation

<div align="center">
  <img src="assets/MultipackParser-MainUI.png" alt="MultipackParser Main Interface" width="700">
  
  <p><em>MultipackParser main interface after startup</em></p>
</div>

---

## Welcome

Welcome to the official documentation for **MultipackParser** — a bridge application that connects [Multipack](https://multiscience.de/multipack-ihre-optimierungssoftware/) palette optimization software with Universal Robots (UR10/UR20) palletizing systems.

## What is MultipackParser?

MultipackParser serves as the communication layer between:

| Component | Role |
|-----------|------|
| **[Multipack](https://multiscience.de/multipack-ihre-optimierungssoftware/)** | Commercial optimization software from Multiscience GmbH that generates optimal palette plans based on box dimensions, palette specifications, and constraints |
| **MultipackParser** | This application — parses palette plan files (`.rob`), provides GUI management, and exposes data via XML-RPC |
| **Universal Robot** | UR10 or UR20 robot running custom in-house palletizing application that queries palette data |

### Workflow Diagram

```
┌──────────────────────────────────────────────────────────────────────────┐
│                           MULTIPACKPARSER WORKFLOW                       │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│   ┌─────────────┐        ┌──────────────────┐        ┌─────────────┐     │
│   │  MULTIPACK  │  .rob  │  MULTIPACKPARSER │  RPC   │  UR ROBOT   │     │
│   │  Software   │ ─────► │    Application   │ ◄────► │   Program   │     │
│   └─────────────┘        └──────────────────┘        └─────────────┘     │
│         │                        │                          │            │
│         ▼                        ▼                          ▼            │
│   ┌─────────────┐        ┌──────────────────┐        ┌─────────────┐     │
│   │ Box/Palette │        │  • USB Loading   │        │ Palletizing │     │
│   │ Dimensions  │        │  • 3D Preview    │        │  Execution  │     │
│   │    Input    │        │  • DB Storage    │        │             │     │
│   └─────────────┘        └──────────────────┘        └─────────────┘     │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

## Key Features

### 📦 Pallet Management
- Load palette plans from USB storage
- View 3D visualization of pallet layouts
- Filter and search pallets by dimensions
- Persistent SQLite database storage

### 🤖 Robot Communication
- Real-time XML-RPC server on port 50000
- Support for both UR10 and UR20 robot models
- Dashboard server commands (port 29999)
- Status monitoring and remote control

### 📊 Status Monitoring
- Robot mode (Manual/Automatic/Remote Control)
- Safety status indicators
- Program state tracking
- Scanner status (UR20: 3 scanners, UR10: 2 scanners)

### 🔒 Security & Settings
- Password-protected settings access
- Encrypted credential storage
- Configurable robot model selection

### 🔊 Audio Notifications
- Configurable sound alerts
- Scanner warning sounds
- Safety event notifications

---

## Documentation Structure

### [📖 Installation Guide](installation.md)
Complete setup instructions including:
- System requirements
- Installation from source
- Pre-built binary installation
- Docker build process
- Troubleshooting common issues

### [📘 User Guide](user-guide.md)
Comprehensive user manual covering:
- Getting started
- Main interface overview
- Loading pallet data
- Robot communication setup
- 3D visualization
- Settings configuration
- Troubleshooting

### [🏗️ Architecture Documentation](architecture.md)
Technical documentation for developers:
- System architecture
- Project structure
- Core components
- Data flow diagrams
- Communication protocols
- Design patterns
- Extension points

### [📡 API Reference](api-reference.md)
Complete XML-RPC API documentation:
- Server configuration
- Common functions (all robot types)
- UR10-specific functions
- UR20-specific functions
- Client implementation examples
- Error handling

---

## Quick Start

### 1. Download binary on Raspberry Pi

```bash
curl -O https://github.com/Snupai/MultipackParser/releases/latest/download/MultipackParser 
```

### 2. Make binary executable

```bash
sudo chmod +x MultipackParser
```

### 3. Verify Installation

```bash
./MultipackParser --version # Display version
./MultipackParser --verbose # Run with debug logging
```

---

## System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| **Python** | 3.12 | 3.12+ |
| **RAM** | 512 MB | 1 GB |
| **Storage** | 300 MB | 500 MB |
| **OS** | Linux | Raspberry Pi OS |

### External Programs required

- [Multipack](https://multiscience.de/multipack-ihre-optimierungssoftware/) (commercial, for generating `.rob` files)

### Development Requirements (For Source/Beta Builds Only)

- Python 3.12+
- pip (Python package manager)

#### Python Dependencies (development only)
- PySide6 >= 6.7.2 (Qt6 GUI framework)
- matplotlib == 3.7.5 (3D visualization)
- cryptography (encrypted settings)
- pygame (audio playback)
- pydub, ffmpeg-python (audio processing)
- python-dotenv (environment configuration)
- requests (HTTP utilities)

---

## Important Notice

> [!IMPORTANT]
> ### Robot-Side Components Required
> 
> This application requires components running on the Universal Robot controller that are **not included** in this repository:
> 
> - **UR Program**: A specific program must be installed on the robot controller
> - **URscript**: Script code that communicates with the XML-RPC server
> 
> These components must be obtained separately and properly configured on your UR10 or UR20 controller. Without them, the robot cannot communicate with MultipackParser.

---

## Getting Help

1. **Check the [User Guide](user-guide.md)** for common workflows and troubleshooting
2. **Review the [Architecture Documentation](architecture.md)** for technical questions
3. **Consult the [API Reference](api-reference.md)** for integration details
4. **Check log files** in the `logs/` directory for error details

---

## License

This project is licensed under the **GNU General Public License v3.0**.

```
Copyright (C) 2025 Yann-Luca Näher

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
```

See the [LICENSE](../LICENSE) file for the complete license text.

---

<div align="center">
  <sub>MultipackParser Documentation • Version 1.7.9</sub>
</div>
