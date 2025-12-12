# Architecture Documentation

This document provides technical documentation for developers working on or integrating with MultipackParser.

## Table of Contents

1. [System Overview](#system-overview)
2. [Project Structure](#project-structure)
3. [Core Components](#core-components)
4. [Data Flow](#data-flow)
5. [Communication Protocols](#communication-protocols)
6. [Database Schema](#database-schema)
7. [Design Patterns](#design-patterns)
8. [Extension Points](#extension-points)

## System Overview

### Architecture Pattern

MultipackParser follows a modular architecture with clear separation of concerns:

```
┌─────────────────────────────────────────┐
│         Main Application                │
│         (main.py)                       │
└──────────────┬──────────────────────────┘
               │
    ┌──────────┴──────────┐
    │                     │
┌───▼────┐         ┌──────▼─────┐
│   UI   │         │   Core     │
│ Layer  │         │  Services  │
└───┬────┘         └──────┬─────┘
    │                     │
    │              ┌──────┴──────┐
    │              │              │
┌───▼──────────┐ ┌─▼──────┐ ┌───▼────────┐
│  Database    │ │ Robot  │ │  Server    │
│  Layer       │ │ Control│ │  Layer     │
└──────────────┘ └────────┘ └────────────┘
```

### Technology Stack

- **GUI Framework**: PySide6 (Qt6)
- **Database**: SQLite
- **Communication**: XML-RPC
- **Visualization**: Matplotlib (3D)
- **Audio**: Pygame
- **Cryptography**: cryptography library

## Project Structure

```
MultipackParser/
├── main.py                 # Application entry point
├── ui_files/              # UI components and resources
│   ├── ui_main_window.py  # Generated UI code
│   ├── MainWindow.ui      # Qt Designer file
│   └── imgs/              # Image resources
├── utils/                 # Core utilities
│   ├── system/            # System-level utilities
│   │   ├── core/          # Core application logic
│   │   ├── config/        # Configuration management
│   │   └── security/      # Security features
│   ├── database/          # Database operations
│   ├── robot/             # Robot control and monitoring
│   ├── server/            # XML-RPC server
│   ├── ui/                # UI helpers and setup
│   ├── message/           # Message management
│   └── audio/             # Audio notifications
└── docs/                  # Documentation
```

## Core Components

### 1. Application Initialization (`utils/system/core/app_initialization.py`)

**Responsibilities**:
- Parse command-line arguments
- Initialize Qt application
- Create splash screen
- Set up initial application state

**Key Functions**:
- `parse_arguments()`: Parse CLI arguments
- `initialize_app()`: Initialize Qt application
- `setup_initial_app_state()`: Configure initial state

### 2. Global Variables (`utils/system/core/global_vars.py`)

**Purpose**: Centralized global state management

**Key Variables**:
- `VERSION`: Application version
- `settings`: Settings instance
- `main_window`: Main window reference
- `robot_ip`: Robot IP address
- Robot status variables
- Pallet data variables

### 3. Settings Management (`utils/system/config/settings.py`)

**Features**:
- JSON-based configuration
- Encrypted password storage
- Persistent settings
- Default value management

**Settings Structure**:
```python
{
    "info": {
        "UR_Model": "UR10" | "UR20",
        "last_restart": "timestamp",
        "number_of_use_cycles": "count"
    },
    "network": {
        # Network settings
    },
    "audio": {
        # Audio preferences
    }
}
```

### 4. Database Layer (`utils/database/`)

**Components**:
- `database.py`: Database connection and operations
- `pallet_data.py`: Pallet data models

**Database**: SQLite (`paletten.db`)

**Key Operations**:
- Create/update pallet records
- Query pallet data
- Filter by dimensions

### 5. Robot Control (`utils/robot/`)

**Components**:
- `robot_control.py`: Robot command interface
- `robot_status_monitor.py`: Status monitoring
- `robot_enums.py`: Status enumerations

**Communication**:
- Dashboard Server (port 29999): Commands
- XML-RPC Server (port 50000): Data exchange

**Status Monitoring**:
- Robot mode (Manual/Automatic/Remote)
- Safety status
- Program state
- Scanner status (UR20)

### 6. Server Layer (`utils/server/`)

**Components**:
- `server.py`: XML-RPC server implementation
- `UR_Common_functions.py`: Common robot functions
- `UR10_Server_functions.py`: UR10-specific functions
- `UR20_Server_functions.py`: UR20-specific functions

**Server Features**:
- Multi-threaded XML-RPC server
- Robot-type specific function registration
- Request logging
- Error handling

### 7. UI Layer (`utils/ui/`)

**Components**:
- `ui_setup.py`: UI initialization and setup
- `ui_helpers.py`: UI utility functions
- `startup_dialogs.py`: Startup dialogs
- `notification_popup.py`: Notification system

**UI Framework**: PySide6 (Qt6)

## Data Flow

### Pallet Data Loading

```
USB Stick → File Reading → Database Update → Global Variables → UI Display
```

1. **USB Detection**: Application checks USB path
2. **File Reading**: Parse pallet data files
3. **Database Update**: Store in SQLite database
4. **Global Variables**: Update global state
5. **UI Update**: Refresh UI components

### Robot Communication

```
Robot ←→ XML-RPC Server ←→ Application Core ←→ UI
```

1. **Robot Request**: Robot calls XML-RPC function
2. **Server Processing**: Server processes request
3. **Application Logic**: Core logic executes
4. **Response**: Return result to robot
5. **UI Update**: Update UI if needed

### Status Monitoring

```
Robot Dashboard → Status Monitor → Global Variables → UI Status Display
```

1. **Polling**: Periodic status checks
2. **Status Update**: Update global variables
3. **UI Refresh**: Update status indicators

## Communication Protocols

> **⚠️ Important**: The communication protocols described below require robot-side components (UR Program and URscript) that are **not included** in this repository. These must be obtained separately and installed on the Universal Robot controller.

### XML-RPC Server

**Port**: 50000 (configurable)

**Robot-Side Requirements**:
- The robot must have a UR Program installed that includes URscript code
- The URscript must be configured to connect to the XML-RPC server
- The robot-side implementation is not provided in this repository

**Common Functions** (all robot types):
- `UR_SetFileName(Artikelnummer)`: Set pallet file name
- `UR_ReadDataFromUsbStick()`: Read data from USB
- `UR_GetPalletData()`: Get pallet data
- `get_available_functions()`: List available functions

**UR10-Specific Functions**:
- `UR_scanner1and2niobild()`: Scanner status
- `UR_scanner1bild()`: Scanner 1 status
- `UR_scanner2bild()`: Scanner 2 status
- `UR_scanner1and2iobild()`: Combined scanner status

**UR20-Specific Functions**:
- `UR_scannerStatus()`: Scanner status (3 scanners)
- `UR_SetActivePalette(palette_number)`: Set active palette
- `UR_RequestPaletteChange()`: Request palette change
- `UR_GetActivePaletteNumber()`: Get active palette
- `UR_GetPaletteStatus()`: Get palette status
- `UR_SetZwischenLageLegen(value)`: Set intermediate layer
- `UR_GetKlemmungAktiv()`: Get clamping status
- `UR_GetScannerOverwrite()`: Get scanner override

### Dashboard Server

**Port**: 29999

**Commands**:
- `play`: Start robot program
- `pause`: Pause robot program
- `stop`: Stop robot program
- `quit`: Quit remote control
- `power on`: Power on robot
- `power off`: Power off robot
- `brake release`: Release brakes

## Database Schema

### Pallet Table

```sql
CREATE TABLE pallets (
    id INTEGER PRIMARY KEY,
    artikelnummer TEXT,
    length INTEGER,
    width INTEGER,
    height INTEGER,
    -- Additional fields
    created_at TIMESTAMP,
    updated_at TIMESTAMP
);
```

### Data Structure

**Global Variables** (from `global_vars.py`):
- `g_PalettenDim`: Palette dimensions [length, width, height]
- `g_PaketDim`: Package dimensions [length, width, height, gap]
- `g_Daten`: Layer data
- `g_PaketPos`: Package positions
- `g_AnzahlPakete`: Number of packages
- `g_AnzLagen`: Number of layers

## Design Patterns

### 1. Singleton Pattern

**Usage**: Settings, Message Manager, Global Variables

**Implementation**: Module-level instances

### 2. Observer Pattern

**Usage**: Status monitoring, UI updates

**Implementation**: Qt signals and slots

### 3. Factory Pattern

**Usage**: Robot-specific function registration

**Implementation**: Dynamic function registration based on robot type

### 4. Strategy Pattern

**Usage**: Robot-type specific implementations

**Implementation**: Separate modules for UR10 and UR20

## Extension Points

### Adding New Robot Types

1. **Create Server Functions Module**
   - `utils/server/URXX_Server_functions.py`

2. **Register Functions**
   - Add to `server.py` registration logic

3. **Update Enums**
   - Add to `robot_enums.py` if needed

4. **Update UI**
   - Add robot-specific UI elements if needed

### Adding New Features

1. **Core Logic**: Add to appropriate `utils/` module
2. **UI Integration**: Update `ui_setup.py` and UI files
3. **Settings**: Add to settings structure
4. **Documentation**: Update relevant docs

### Customizing UI

1. **Edit UI Files**: Use Qt Designer (`pyside6-designer`)
2. **Convert UI**: Use `pyside6-uic` to generate Python
3. **Update Resources**: Modify `.qrc` files for resources

## Threading Model

### Main Thread
- UI operations
- Qt event loop
- User interactions

### Background Threads
- Status monitoring
- Audio processing
- Server requests (XML-RPC handles threading)

## Error Handling

### Exception Handling Strategy

1. **Application Level**: Global exception handler
2. **Module Level**: Try-except blocks
3. **Logging**: All errors logged to file
4. **User Feedback**: Status messages and dialogs

### Logging

**Log Levels**:
- DEBUG: Detailed information (verbose mode)
- INFO: General information
- WARNING: Warning messages
- ERROR: Error conditions
- CRITICAL: Critical failures

**Log Files**:
- Application: `logs/multipack_parser_*.log`
- Server: `logs/server_*.log`

## Performance Considerations

1. **Lazy Loading**: Heavy imports delayed until needed
2. **Caching**: Settings and data cached in memory
3. **Efficient Queries**: Database queries optimized
4. **Threading**: Background operations don't block UI

## Security

1. **Password Protection**: Settings encrypted
2. **Input Validation**: All inputs validated
3. **Network Security**: XML-RPC on local network only
4. **File Access**: Controlled file system access

## Testing

### Manual Testing

1. **Unit Tests**: Test individual functions
2. **Integration Tests**: Test component interactions
3. **System Tests**: Test full workflows

### Test Scenarios

- Pallet data loading
- Robot communication
- Status monitoring
- Error handling
- UI interactions

## Future Enhancements

Potential areas for improvement:
- Automated testing framework
- Configuration file validation
- Enhanced error recovery
- Performance optimizations
- Additional robot support

## References

- [PySide6 Documentation](https://doc.qt.io/qtforpython/)
- [XML-RPC Specification](https://www.xmlrpc.com/spec)
- [Universal Robots API](https://www.universal-robots.com/)

