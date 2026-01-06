# Architecture Documentation

This document provides comprehensive technical documentation for developers working on or integrating with MultipackParser.

---

## Table of Contents

1. [System Overview](#system-overview)
2. [Project Structure](#project-structure)
3. [Core Components](#core-components)
4. [Data Flow](#data-flow)
5. [Communication Protocols](#communication-protocols)
6. [Database Schema](#database-schema)
7. [Design Patterns](#design-patterns)
8. [Threading Model](#threading-model)
9. [Extension Points](#extension-points)
10. [Development Guidelines](#development-guidelines)

---

## System Overview

### Architecture Pattern

MultipackParser follows a **modular layered architecture** with clear separation of concerns:

```
+------------------------------------------------------------------+
|                        APPLICATION LAYER                          |
|  +----------------------------------------------------------+    |
|  |                     main.py (Entry Point)                 |    |
|  +----------------------------------------------------------+    |
+------------------------------------------------------------------+
                                |
+------------------------------------------------------------------+
|                         UI LAYER (PySide6/Qt6)                    |
|  +------------------+  +------------------+  +----------------+  |
|  |   ui_setup.py    |  |   ui_helpers.py  |  | notification.py|  |
|  +------------------+  +------------------+  +----------------+  |
+------------------------------------------------------------------+
                                |
+------------------------------------------------------------------+
|                        SERVICE LAYER                              |
|  +-------------+  +---------------+  +-------------+  +-------+  |
|  |   server/   |  |    robot/     |  |  database/  |  | audio/|  |
|  | XML-RPC Srv |  | Status Monitor|  |   SQLite    |  | Sound |  |
|  +-------------+  +---------------+  +-------------+  +-------+  |
+------------------------------------------------------------------+
                                |
+------------------------------------------------------------------+
|                         CORE LAYER                                |
|  +------------------+  +------------------+  +----------------+   |
|  |   global_vars.py |  |   settings.py    |  | app_control.py |   |
|  +------------------+  +------------------+  +----------------+   |
+------------------------------------------------------------------+
```

### Technology Stack

| Component | Technology | Purpose |
|-----------|------------|---------|
| **GUI Framework** | PySide6 (Qt6) | Cross-platform desktop interface |
| **Database** | SQLite | Local data persistence |
| **Communication** | XML-RPC | Robot data exchange |
| **Visualization** | Matplotlib | 3D pallet visualization |
| **Audio** | Pygame | Sound notifications |
| **Security** | cryptography | Encrypted settings |

---

## Project Structure

```
MultipackParser/
|
+-- main.py                     # Application entry point
+-- build.py                    # Build script for ARM64 binary
+-- requirements.txt            # Python dependencies
|
+-- ui_files/                   # UI components and resources
|   +-- ui_main_window.py       # Generated UI code (from .ui file)
|   +-- MainWindow.ui           # Qt Designer file
|   +-- MainWindowResources.qrc # Qt resource file
|   +-- MainWindowResources_rc.py # Generated resource code
|   +-- BlinkingLabel.py        # Custom UI widget
|   +-- imgs/                   # Image resources
|       +-- UR10/               # UR10-specific images
|       +-- UR20/               # UR20-specific images
|
+-- utils/                      # Core application modules
|   |
|   +-- system/                 # System-level utilities
|   |   +-- core/               # Core application logic
|   |   |   +-- global_vars.py  # Global state management
|   |   |   +-- app_initialization.py  # App startup
|   |   |   +-- app_control.py  # App lifecycle control
|   |   +-- config/             # Configuration management
|   |   |   +-- settings.py     # Settings persistence
|   |   |   +-- logging_config.py # Logging setup
|   |   +-- security/           # Security features
|   |
|   +-- database/               # Database operations
|   |   +-- database.py         # SQLite operations
|   |   +-- pallet_data.py      # Pallet data models
|   |
|   +-- robot/                  # Robot control and monitoring
|   |   +-- robot_control.py    # Robot command interface
|   |   +-- robot_status_monitor.py # Status polling
|   |   +-- robot_enums.py      # Status enumerations
|   |
|   +-- server/                 # XML-RPC server
|   |   +-- server.py           # Server implementation
|   |   +-- UR_Common_functions.py   # Common robot functions
|   |   +-- UR10_Server_functions.py # UR10-specific functions
|   |   +-- UR20_Server_functions.py # UR20-specific functions
|   |
|   +-- ui/                     # UI helpers and setup
|   |   +-- ui_setup.py         # UI initialization
|   |   +-- ui_helpers.py       # Utility functions
|   |   +-- startup_dialogs.py  # Startup dialogs
|   |   +-- notification_popup.py # Notifications
|   |
|   +-- message/                # Message management
|   |   +-- message_manager.py  # Message handling
|   |   +-- status_manager.py   # Status updates
|   |
|   +-- audio/                  # Audio notifications
|       +-- audio.py            # Sound playback
|
+-- docs/                       # Documentation
|   +-- index.md
|   +-- installation.md
|   +-- user-guide.md
|   +-- architecture.md
|   +-- api-reference.md
|   +-- assets/                 # Documentation images
|
+-- logs/                       # Application logs (auto-generated)
```

---

## Core Components

### 1. Application Entry Point (`main.py`)

**Responsibilities:**
- Parse command-line arguments
- Handle `--version` and `--license` flags early (no heavy imports)
- Initialize Qt application with proper environment
- Coordinate startup sequence with splash screen
- Launch main window and event loop

**Startup Sequence:**
```
1. Parse arguments
2. Handle --version/--license (early exit)
3. Set environment variables (OpenGL, platform)
4. Initialize Qt application
5. Show splash screen
6. Create database
7. Initialize main window
8. Load settings
9. Update database from USB
10. Setup UI components
11. Start background tasks
12. Show main window
```

### 2. Global Variables (`utils/system/core/global_vars.py`)

**Purpose:** Centralized state management for application-wide variables

**Key Variables:**

| Variable | Type | Purpose |
|----------|------|---------|
| `VERSION` | str | Application version (1.7.9) |
| `settings` | Settings | Settings instance |
| `main_window` | QWidget | Main window reference |
| `robot_ip` | str | Robot IP address |
| `current_robot_mode` | RobotMode | Current robot mode |
| `current_safety_status` | SafetyStatus | Current safety status |
| `g_PalettenDim` | List[int] | Palette dimensions [L, W, H] |
| `g_PaketDim` | List[int] | Package dimensions [L, W, H, gap] |
| `g_PaketPos` | List[List[int]] | Package positions |
| `UR20_active_palette` | int | Active palette (1 or 2) |

**Data Structure Constants:**

```python
# List indices for data structure navigation
LI_PALETTE_DATA = 0       # Palette data index
LI_PACKAGE_DATA = 1       # Package data index
LI_LAYERTYPES = 2         # Layer types index
LI_NUMBER_OF_LAYERS = 3   # Number of layers index

# Position values indices (9 values per position)
LI_POSITION_XP = 0        # X position (pick)
LI_POSITION_YP = 1        # Y position (pick)
LI_POSITION_AP = 2        # Angle (pick)
LI_POSITION_XD = 3        # X position (drop)
LI_POSITION_YD = 4        # Y position (drop)
LI_POSITION_AD = 5        # Angle (drop)
LI_POSITION_NOP = 6       # Number of packages
LI_POSITION_XVEC = 7      # X vector
LI_POSITION_YVEC = 8      # Y vector
```

### 3. Settings Management (`utils/system/config/settings.py`)

**Features:**
- JSON-based configuration persistence
- Encrypted password storage
- Default value management
- Robot model configuration

**Settings Structure:**
```python
{
    "info": {
        "UR_Model": "UR10" | "UR20",
        "last_restart": "timestamp",
        "number_of_use_cycles": int
    },
    "audio": {
        "muted": bool,
        "volume": float
    },
    "admin": {
        "scanner_warning_sound_file": str
    }
}
```

### 4. Database Layer (`utils/database/`)

**Components:**
- `database.py`: Connection management and CRUD operations
- `pallet_data.py`: Pallet data models and parsing

**Database:** SQLite (`paletten.db`)

**Key Functions:**
- `create_database()`: Initialize database schema
- `load_from_database(file_name)`: Load pallet data by filename
- `update_database_from_usb()`: Scan USB and update database

### 5. Robot Control (`utils/robot/`)

**Components:**

| Module | Purpose |
|--------|---------|
| `robot_control.py` | Command interface for robot operations |
| `robot_status_monitor.py` | Continuous status polling |
| `robot_enums.py` | Enumerations for robot states |

**Enumerations:**
```python
class RobotMode(Enum):
    UNKNOWN = 0
    MANUAL = 1
    AUTOMATIC = 2
    REMOTE_CONTROL = 3

class SafetyStatus(Enum):
    UNKNOWN = 0
    NORMAL = 1
    REDUCED = 2
    PROTECTIVE_STOP = 3
    # ...

class ProgramState(Enum):
    UNKNOWN = 0
    STOPPED = 1
    PLAYING = 2
    PAUSED = 3
```

### 6. Server Layer (`utils/server/`)

**Components:**

| Module | Purpose |
|--------|---------|
| `server.py` | XML-RPC server implementation |
| `UR_Common_functions.py` | Functions for all robot types |
| `UR10_Server_functions.py` | UR10-specific functions |
| `UR20_Server_functions.py` | UR20-specific functions |

**Server Configuration:**
- **Protocol:** XML-RPC
- **Port:** 50000
- **Host:** Localhost
- **Threading:** Multi-threaded request handling

### 7. UI Layer (`utils/ui/`)

**Components:**

| Module | Purpose |
|--------|---------|
| `ui_setup.py` | Complete UI initialization |
| `ui_helpers.py` | Utility functions |
| `startup_dialogs.py` | Palette selection dialogs |
| `notification_popup.py` | Notification display |

**Key Setup Functions:**
- `initialize_main_window()`: Create main window
- `setup_input_validation()`: Configure input validators
- `connect_signal_handlers()`: Wire up Qt signals
- `setup_components()`: Initialize UI components
- `start_background_tasks()`: Launch monitoring threads

---

## Data Flow

### System Context

```
+----------+        +------------------+        +-----------+
| Multipack|  .rob  | MultipackParser  |  RPC   | UR Robot  |
| Software | -----> |   Application    | <----> |  Program  |
+----------+        +------------------+        +-----------+
                            |
                            v
                    +---------------+
                    | SQLite DB     |
                    | (paletten.db) |
                    +---------------+
```

### Pallet Data Loading Flow

```
1. USB Detection
   +-- Scan PATH_USB_STICK for .rob files

2. File Parsing
   +-- Parse palette plan data
   +-- Extract dimensions, positions, layers

3. Database Storage
   +-- Store in SQLite database
   +-- Include metadata and timestamps

4. Global State Update
   +-- Update global_vars with current data
   +-- g_PalettenDim, g_PaketDim, g_PaketPos, etc.

5. UI Refresh
   +-- Update display labels
   +-- Refresh 3D visualization
```

### Robot Communication Flow

```
1. Robot Request (XML-RPC)
   +-- Robot calls function (e.g., UR_GetPalletData)

2. Server Processing
   +-- Route to appropriate function handler
   +-- Execute business logic

3. Data Retrieval
   +-- Access global_vars or database
   +-- Transform data if needed

4. Response
   +-- Return data to robot via XML-RPC
   +-- Log request/response

5. UI Update (if needed)
   +-- Update status display
   +-- Refresh indicators
```

### Status Monitoring Flow

```
1. Status Monitor Thread
   +-- Poll robot dashboard server (port 29999)
   +-- Get mode, safety, program state

2. Global State Update
   +-- Update current_robot_mode
   +-- Update current_safety_status
   +-- Update current_program_state

3. Signal Emission
   +-- Qt signals for UI updates

4. UI Refresh
   +-- Update status indicators
   +-- Show warnings if needed
```

---

## Communication Protocols

### XML-RPC Server (Port 50000)

> [!IMPORTANT]
> The XML-RPC server provides the interface for robot communication. Robot-side URscript code (not included) must call these functions.

**Function Categories:**

| Category | Functions |
|----------|-----------|
| **Common** | UR_SetFileName, UR_ReadDataFromUsbStick, UR_Palette, UR_Karton, etc. |
| **UR10** | UR10_scanner1bild, UR10_scanner2bild, etc. |
| **UR20** | UR20_scannerStatus, UR20_SetActivePalette, UR20_RequestPaletteChange, etc. |

See [API Reference](api-reference.md) for complete function documentation.

### Dashboard Server (Port 29999)

The application connects to the UR Dashboard Server for robot control:

| Command | Action |
|---------|--------|
| `play` | Start program |
| `pause` | Pause program |
| `stop` | Stop program |
| `power on/off` | Power control |
| `brake release` | Release brakes |

---

## Database Schema

The application uses SQLite with a normalized schema consisting of 8 tables linked by foreign keys.

### Entity Relationship

```
paletten_metadata (1) ──┬── (N) daten
                        ├── (1) paletten_dim
                        ├── (1) paket_dim
                        ├── (N) lage_zuordnung
                        ├── (N) zwischenlagen
                        ├── (N) pakete_zuordnung
                        └── (N) paket_pos
```

### Tables

#### `paletten_metadata` (Main Table)

Primary table storing metadata for each palette plan file.

```sql
CREATE TABLE paletten_metadata (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    paket_quer INTEGER,              -- Package orientation (1 = default)
    center_of_gravity_x REAL,        -- CoG X coordinate
    center_of_gravity_y REAL,        -- CoG Y coordinate
    center_of_gravity_z REAL,        -- CoG Z coordinate
    lage_arten INTEGER,              -- Number of layer types
    anz_lagen INTEGER,               -- Total number of layers
    anzahl_pakete INTEGER,           -- Number of packages/picks
    file_timestamp REAL,             -- File modification timestamp
    file_name TEXT                   -- Original .rob filename
);

CREATE INDEX idx_file_name ON paletten_metadata(file_name);
```

#### `daten` (Raw Data)

Stores the complete 2D data array from the `.rob` file.

```sql
CREATE TABLE daten (
    id INTEGER PRIMARY KEY,
    metadata_id INTEGER,             -- FK to paletten_metadata
    row_index INTEGER,               -- Row in 2D array
    col_index INTEGER,               -- Column in 2D array
    value INTEGER,                   -- Cell value
    FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
);
```

#### `paletten_dim` (Palette Dimensions)

```sql
CREATE TABLE paletten_dim (
    id INTEGER PRIMARY KEY,
    metadata_id INTEGER,             -- FK to paletten_metadata
    length INTEGER,                  -- Palette length (mm)
    width INTEGER,                   -- Palette width (mm)
    height INTEGER,                  -- Palette height (mm)
    FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
);
```

#### `paket_dim` (Package Dimensions)

```sql
CREATE TABLE paket_dim (
    id INTEGER PRIMARY KEY,
    metadata_id INTEGER,             -- FK to paletten_metadata
    length INTEGER,                  -- Package length (mm)
    width INTEGER,                   -- Package width (mm)
    height INTEGER,                  -- Package height (mm)
    gap INTEGER,                     -- Gap between packages (mm)
    weight REAL,                     -- Package weight (kg)
    einzelpaket_laengs INTEGER,      -- Single package lengthwise flag
    FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
);
```

#### `lage_zuordnung` (Layer Assignments)

Maps each layer to its layer type.

```sql
CREATE TABLE lage_zuordnung (
    id INTEGER PRIMARY KEY,
    metadata_id INTEGER,             -- FK to paletten_metadata
    lage_index INTEGER,              -- Layer index (0-based)
    value INTEGER,                   -- Layer type ID
    FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
);
```

#### `zwischenlagen` (Intermediate Layers)

Flags for intermediate layer placement.

```sql
CREATE TABLE zwischenlagen (
    id INTEGER PRIMARY KEY,
    metadata_id INTEGER,             -- FK to paletten_metadata
    lage_index INTEGER,              -- Layer index (0-based)
    value INTEGER,                   -- 1 = place intermediate layer after
    FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
);
```

#### `pakete_zuordnung` (Package Counts per Layer Type)

```sql
CREATE TABLE pakete_zuordnung (
    id INTEGER PRIMARY KEY,
    metadata_id INTEGER,             -- FK to paletten_metadata
    lage_index INTEGER,              -- Layer type index
    value INTEGER,                   -- Number of packages in this layer type
    FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
);
```

#### `paket_pos` (Package Positions)

Pick and place coordinates for each package.

```sql
CREATE TABLE paket_pos (
    id INTEGER PRIMARY KEY,
    metadata_id INTEGER,             -- FK to paletten_metadata
    paket_index INTEGER,             -- Package index (0-based)
    xp INTEGER,                      -- X position (pick)
    yp INTEGER,                      -- Y position (pick)
    ap INTEGER,                      -- Angle (pick)
    xd INTEGER,                      -- X position (drop)
    yd INTEGER,                      -- Y position (drop)
    ad INTEGER,                      -- Angle (drop)
    nop INTEGER,                     -- Number of packages
    xvec INTEGER,                    -- X vector
    yvec INTEGER,                    -- Y vector
    FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
);
```

### Key Database Functions

| Function | Purpose |
|----------|---------|
| `create_database()` | Initialize schema and run migrations |
| `save_to_database()` | Parse .rob file and save all data |
| `load_from_database()` | Load palette data by file name or ID |
| `list_available_files()` | List all stored .rob files |
| `find_palettplan()` | Search by package dimensions |
| `update_box_dimensions()` | Update height/weight for a file |
| `get_box_weight()` / `get_box_height()` | Retrieve stored values |

### Data Integrity

- **Foreign Keys**: All child tables reference `paletten_metadata(id)` with `ON DELETE CASCADE`
- **Index**: `idx_file_name` on `paletten_metadata(file_name)` for fast lookups
- **Migrations**: Automatic column additions for `weight` and `einzelpaket_laengs`

---

## Design Patterns

### 1. Singleton Pattern

**Usage:** Settings, MessageManager, Global Variables

**Implementation:** Module-level instances

```python
# In global_vars.py
settings: Optional['Settings'] = None
message_manager: Optional['MessageManager'] = None
```

### 2. Observer Pattern

**Usage:** Status monitoring, UI updates

**Implementation:** Qt signals and slots

```python
class ScannerSignals(QObject):
    status_changed = Signal(str, str)

scanner_signals.status_changed.connect(update_scanner_display)
```

### 3. Factory Pattern

**Usage:** Robot-specific function registration

**Implementation:** Dynamic function registration based on robot type

```python
if robot_type == "UR10":
    register_ur10_functions(server)
elif robot_type == "UR20":
    register_ur20_functions(server)
```

### 4. Strategy Pattern

**Usage:** Robot-type specific implementations

**Implementation:** Separate modules for UR10 and UR20

```
UR_Common_functions.py  # Shared logic
UR10_Server_functions.py  # UR10 strategy
UR20_Server_functions.py  # UR20 strategy
```

---

## Threading Model

### Main Thread
- Qt event loop
- UI operations
- User interactions

### Background Threads

| Thread | Purpose |
|--------|---------|
| **Status Monitor** | Poll robot status continuously |
| **Audio Player** | Non-blocking sound playback |
| **Safety Monitor** | Monitor safety conditions |

### XML-RPC Server
- Built-in threading for request handling
- Each request handled in separate thread

### Thread Safety

- Use Qt signals for cross-thread UI updates
- Global variables accessed from multiple threads
- Database operations are serialized

---

## Extension Points

### Adding New Robot Types

1. **Create Server Functions Module**
   ```python
   # utils/server/URXX_Server_functions.py
   def URXX_specific_function():
       pass
   ```

2. **Register Functions in Server**
   ```python
   # In server.py
   if robot_type == "URXX":
       register_urxx_functions(server)
   ```

3. **Update Enums if Needed**
   ```python
   # In robot_enums.py
   class RobotModel(Enum):
       UR10 = "UR10"
       UR20 = "UR20"
       URXX = "URXX"  # New model
   ```

4. **Update UI if Needed**
   - Add robot-specific UI elements
   - Update settings options

### Adding New Features

1. **Core Logic:** Add to appropriate `utils/` module
2. **UI Integration:** Update `ui_setup.py`
3. **Settings:** Add to settings structure
4. **Documentation:** Update relevant docs

### Customizing UI

1. **Edit UI Files:** Use Qt Designer
   ```bash
   pyside6-designer
   ```

2. **Convert to Python:**
   ```bash
   pyside6-uic MainWindow.ui -o ui_main_window.py
   ```

3. **Update Resources:**
   ```bash
   pyside6-rcc MainWindowResources.qrc -o MainWindowResources_rc.py
   ```

> [!WARNING]
> After running `pyside6-uic`, change:
> `import MainWindowResources_rc` to `from . import MainWindowResources_rc`

---

## Development Guidelines

### Code Style

- Follow PEP 8 guidelines
- Use type hints for function signatures
- Document functions with docstrings
- Use meaningful variable names

### Logging

**Log Levels:**

| Level | Usage |
|-------|-------|
| DEBUG | Detailed debugging information |
| INFO | General operational information |
| WARNING | Warning conditions |
| ERROR | Error conditions |
| CRITICAL | Critical failures |

**Log Files:**
- `logs/multipack_parser_*.log` - Application logs
- `logs/server_*.log` - Server logs

### Error Handling

1. **Application Level:** Global exception handler in main.py
2. **Module Level:** Try-except blocks with logging
3. **User Feedback:** Status messages and dialogs

### Performance Considerations

1. **Lazy Loading:** Heavy imports delayed until needed
2. **Caching:** Settings and data cached in memory
3. **Efficient Queries:** Database queries optimized
4. **Non-blocking UI:** Background threads for long operations

### Security

1. **Password Protection:** Settings encrypted with cryptography
2. **Input Validation:** All inputs validated
3. **Network Security:** XML-RPC on local network only
4. **File Access:** Controlled file system access

---

## References

- [PySide6 Documentation](https://doc.qt.io/qtforpython/)
- [XML-RPC Specification](https://www.xmlrpc.com/spec)
- [Universal Robots Documentation](https://www.universal-robots.com/)
- [Python Type Hints](https://docs.python.org/3/library/typing.html)

---

<div align="center">
  <sub>MultipackParser Architecture Documentation - Version 1.7.9</sub>
</div>
