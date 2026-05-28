# Python to C++ Migration Guide

## Overview

This document maps Python components to their C++ equivalents for the MultipackParser rewrite.

## Component Mapping

### Core Components

| Python Module | C++ Class | Header |
|--------------|-----------|--------|
| `main.py` | `Application` | `include/multipack/core/Application.h` |
| `utils/system/core/global_vars.py` | `GlobalState` | `include/multipack/core/GlobalState.h` |
| - | `AppInitializer` | `include/multipack/core/AppInitializer.h` |
| - | `ArgumentParser` | `include/multipack/core/ArgumentParser.h` |

### Configuration

| Python Module | C++ Class | Header |
|--------------|-----------|--------|
| `utils/system/config/settings.py` | `SettingsManager` | `include/multipack/config/SettingsManager.h` |
| `utils/system/config/logging_config.py` | `LoggingConfig` | `include/multipack/config/LoggingConfig.h` |
| - | `ConfigDefaults` | `include/multipack/config/ConfigDefaults.h` |

#### Logging Parity Contract

The C++ `LoggingConfig` matches the original Python `logging_config.py` on the
following observable behaviors:

| Property | Python | C++ |
|---|---|---|
| App logger name | `multipack_parser` | `multipack_parser` |
| Server logger name | `server` | `server` (via Qt category `serverLog`) |
| Record format | `%(asctime)s - %(name)s - %(levelname)s - %(message)s` | `yyyy-MM-dd HH:mm:ss,zzz - <name> - <LEVEL> - <message>` |
| App level | DEBUG if verbose, else INFO | Same (driven by `MULTIPACK_VERBOSE` / `--verbose`) |
| Server level | Always DEBUG | Always DEBUG |
| File rotation | `RotatingFileHandler` 5 MB / 5 backups | Custom rolling sink, 5 MB / 5 backups |
| Active file names | `multipack_parser_<timestamp>.log`, `server_<timestamp>.log` | `multipack_parser.log`, `server.log` (with `.1`..`.5` backups) |
| Fallback on failure | User home directory | User home directory |
| Encoding | UTF-8 | UTF-8 |
| Console mirror | stdout via `StreamHandler` | stderr |

**Notes:**
- File naming changed from per-launch timestamped files to stable names with
  numeric backup suffixes. This is required for true size-based rotation; the
  Python implementation produced timestamped files *per process launch* but
  still rolled within each file via `RotatingFileHandler`.
- Server-channel routing in C++ is explicit via the `multipack::config::serverLog`
  Qt logging category; downstream code must emit through `qCDebug(serverLog)` /
  `qCInfo(serverLog)` / `qCWarning(serverLog)` / `qCCritical(serverLog)` to be
  routed to the server sink. There is no substring-based heuristic.

### Database

| Python Module | C++ Class | Header |
|--------------|-----------|--------|
| `utils/database/database.py` | `DatabaseManager` | `include/multipack/database/DatabaseManager.h` |
| - | `PaletData` | `include/multipack/database/PaletData.h` |
| - | `DatabaseMigrations` | `include/multipack/database/DatabaseMigrations.h` |
| - | `DatabaseModels` | `include/multipack/database/DatabaseModels.h` |

### Network/Server

| Python Module | C++ Class | Header |
|--------------|-----------|--------|
| `utils/server/server.py` | `XmlRpcServer` | `include/multipack/network/XmlRpcServer.h` |
| `utils/server/UR_Common_functions.py` | `URCommonFunctions` | `include/multipack/network/URCommonFunctions.h` |
| `utils/server/UR10_Server_functions.py` | `UR10ServerFunctions` | `include/multipack/network/UR10ServerFunctions.h` |
| `utils/server/UR20_Server_functions.py` | `UR20ServerFunctions` | `include/multipack/network/UR20ServerFunctions.h` |
| - | `RpcMethodRegistry` | `include/multipack/network/RpcMethodRegistry.h` |

### Robot Control

| Python Module | C++ Class | Header |
|--------------|-----------|--------|
| `utils/robot/robot_control.py` | `RobotController` | `include/multipack/robot/RobotController.h` |
| - | `RobotStatusMonitor` | `include/multipack/robot/RobotStatusMonitor.h` |
| - | `DashboardClient` | `include/multipack/robot/DashboardClient.h` |
| - | `RobotEnums` | `include/multipack/robot/RobotEnums.h` |
| - | `RobotCommands` | `include/multipack/robot/RobotCommands.h` |

### UI

| Python Module | C++ Class | Header |
|--------------|-----------|--------|
| `utils/ui/ui_setup.py` | `MainWindow` | `include/multipack/ui/MainWindow.h` |
| `ui_files/password_entry_widget.py` | `PasswordDialog` | `include/multipack/ui/PasswordDialog.h` |
| `utils/ui/visualization_3d.py` | `VisualizationWidget` | `include/multipack/ui/VisualizationWidget.h` |
| `ui_files/BlinkingLabel.py` | `BlinkingLabel` | `include/multipack/ui/BlinkingLabel.h` |

### Audio

| Python Module | C++ Class | Header |
|--------------|-----------|--------|
| `utils/audio/audio.py` | `AudioManager` | `include/multipack/audio/AudioManager.h` |
| - | `AudioQueue` | `include/multipack/audio/AudioQueue.h` |
| - | `SafetyMonitor` | `include/multipack/audio/SafetyMonitor.h` |

## Data Structure Mapping

### Global Variables → GlobalState

Python:
```python
# global_vars.py
g_PalettenDim = [0, 0, 0, 0]
g_PaketDim = [0, 0, 0, 0]
g_PaketPos = []
g_Daten = []
```

C++:
```cpp
// GlobalState.h
class GlobalState {
    QVector<double> m_paletteDimensions;
    QVector<double> m_packageDimensions;
    QVector<QVector<double>> m_positions;
    QVector<QVariant> m_data;
};
```

### Index Constants

Python:
```python
LI_PALETTE_DATA = 0
LI_PACKAGE_DATA = 1
LI_LAYERTYPES = 2
LI_NUMBER_OF_LAYERS = 3
```

C++:
```cpp
namespace DataIndices {
    constexpr int PaletteData = 0;
    constexpr int PackageData = 1;
    constexpr int LayerTypes = 2;
    constexpr int NumberOfLayers = 3;
}
```

### Position Array

Python (9 values per position):
```python
[x_pick, y_pick, angle_pick, x_drop, y_drop, angle_drop, count, x_vec, y_vec]
```

C++:
```cpp
struct Position {
    double xPick, yPick, anglePick;
    double xDrop, yDrop, angleDrop;
    int count;
    double xVector, yVector;
};
```

## Pattern Translations

### Singleton

Python:
```python
# Often using module-level variables
from utils.system.core import global_vars
```

C++:
```cpp
// Meyers' singleton
GlobalState& GlobalState::instance() {
    static GlobalState instance;
    return instance;
}
```

### Qt Signals/Slots

Python:
```python
# PySide6
self.signal_name.connect(self.slot_method)
```

C++:
```cpp
// Qt6
connect(sender, &Sender::signalName, receiver, &Receiver::slotMethod);
```

### Error Handling

Python:
```python
try:
    result = risky_operation()
except Exception as e:
    logging.error(f"Error: {e}")
```

C++:
```cpp
try {
    auto result = riskyOperation();
} catch (const std::exception& e) {
    qCritical() << "Error:" << e.what();
}
```

### Threading

Python:
```python
import threading
thread = threading.Thread(target=worker)
thread.start()
```

C++:
```cpp
QThread* thread = new QThread;
Worker* worker = new Worker;
worker->moveToThread(thread);
connect(thread, &QThread::started, worker, &Worker::process);
thread->start();
```

## Key Differences

### String Handling

| Python | C++ |
|--------|-----|
| `str` | `QString` |
| `f"value: {x}"` | `QString("value: %1").arg(x)` |
| `s.split(',')` | `s.split(',')` |
| `s.strip()` | `s.trimmed()` |

### Collections

| Python | C++ |
|--------|-----|
| `list` | `QVector<T>` or `QList<T>` |
| `dict` | `QMap<K,V>` or `QHash<K,V>` |
| `set` | `QSet<T>` |

### File I/O

| Python | C++ |
|--------|-----|
| `open(path, 'r')` | `QFile file(path); file.open(...)` |
| `with open(...) as f:` | RAII with QFile |
| `json.load(f)` | `QJsonDocument::fromJson(...)` |

## Implementation Status

### Completed (Stub)
- [x] Core application framework
- [x] Configuration management
- [x] Database models
- [x] Network/RPC structure
- [x] Robot control framework
- [x] Audio system
- [x] Message system
- [x] Utility functions

### Pending Implementation
- [ ] Full MainWindow UI logic
- [ ] VTK 3D visualization
- [ ] Complete database CRUD
- [ ] XML-RPC method handlers
- [ ] Robot command execution
- [ ] .rob file parsing

## Testing Strategy

1. **Unit Tests**: Individual class testing
2. **Integration Tests**: Component interaction
3. **UI Tests**: Qt Test framework
4. **Comparison Tests**: Run Python and C++ side-by-side

## Migration Steps

1. Implement core classes first
2. Port database operations
3. Add XML-RPC methods one at a time
4. Build UI incrementally
5. Integrate 3D visualization last
6. Parallel testing with Python version
