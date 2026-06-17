# MultipackParser C++ Architecture

## Overview

The C++ version follows a layered architecture similar to the Python version, with improvements for type safety and performance.

## Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                        main.cpp                              │
│                    (Entry Point)                             │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Core Layer                                │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ Application │  │ GlobalState │  │ AppInitializer      │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    UI Layer                                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ MainWindow  │  │PasswordDlg │  │ VisualizationWidget │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                  Service Layer                               │
│  ┌──────────┐  ┌───────────┐  ┌─────────┐  ┌─────────────┐  │
│  │ Database │  │ XmlRpc    │  │ Robot   │  │ Audio       │  │
│  │ Manager  │  │ Server    │  │ Control │  │ Manager     │  │
│  └──────────┘  └───────────┘  └─────────┘  └─────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                  Config/Utils Layer                          │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐ │
│  │ Settings  │  │ Logging   │  │ String    │  │ Socket    │ │
│  │ Manager   │  │ Config    │  │ Utils     │  │ Utils     │ │
│  └───────────┘  └───────────┘  └───────────┘  └───────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## Namespaces

All code is organized under the `multipack` namespace:

```cpp
namespace multipack {
    namespace core { }      // Application core
    namespace ui { }        // User interface
    namespace database { }  // Database operations
    namespace network { }   // XML-RPC server
    namespace robot { }     // Robot communication
    namespace audio { }     // Audio playback
    namespace message { }   // Messaging system
    namespace config { }    // Configuration
    namespace system { }    // System utilities
    namespace utils { }     // Helper utilities
}
```

## Class Relationships

### Core Classes

```
Application (QApplication)
    │
    ├── AppInitializer
    │   ├── SettingsManager
    │   ├── DatabaseManager
    │   ├── XmlRpcServer
    │   ├── RobotController
    │   └── AudioManager
    │
    └── GlobalState (Singleton)
```

### UI Classes

```
MainWindow (QMainWindow)
    ├── Ui::MainWindow (Generated from .ui)
    ├── VisualizationWidget
    ├── BlinkingLabel
    └── PasswordDialog
```

### Service Classes

```
DatabaseManager
    ├── PaletData
    └── DatabaseMigrations

XmlRpcServer
    ├── RpcMethodRegistry
    ├── UR10ServerFunctions
    ├── UR20ServerFunctions
    └── URCommonFunctions

RobotController
    ├── DashboardClient
    └── RobotStatusMonitor

AudioManager
    ├── AudioQueue
    └── SafetyMonitor
```

## Threading Model

| Component | Thread | Purpose |
|-----------|--------|---------|
| Qt Event Loop | Main | UI updates |
| XmlRpcServer | Background | Handle RPC requests |
| RobotStatusMonitor | Background | Poll robot status |
| AudioManager | Background | Audio playback |
| SafetyMonitor | Background | Monitor safety state |

### Thread Safety

- Qt signals/slots for cross-thread communication
- QMutex for shared data protection
- Smart pointers (std::unique_ptr, std::shared_ptr) for ownership

## Memory Management

- QObject parent-child for Qt objects
- std::unique_ptr for exclusive ownership
- RAII pattern for resource management
- No raw `new`/`delete` outside of Qt parent system

## Data Flow

### Palette Loading

```
USB Stick → FileOperations::parseRobFile()
         → DatabaseManager::save()
         → GlobalState::update()
         → MainWindow::refresh()
```

### Robot Communication

```
Robot Request → XmlRpcServer::handleRequest()
             → URCommonFunctions/UR10Functions/UR20Functions
             → GlobalState::getData()
             → Return XML-RPC Response
```

### Status Monitoring

```
Timer Tick → RobotStatusMonitor::poll()
          → DashboardClient::query()
          → GlobalState::updateRobotStatus()
          → Signal: robotStatusChanged()
          → MainWindow::updateStatusDisplay()
```

## Key Design Decisions

### Singleton Pattern

GlobalState uses singleton pattern for centralized state:

```cpp
GlobalState& GlobalState::instance() {
    static GlobalState instance;
    return instance;
}
```

### Observer Pattern

Qt signals/slots implement observer pattern:

```cpp
// Robot controller emits status changes
connect(robotController, &RobotController::statusChanged,
        mainWindow, &MainWindow::onRobotStatusChanged);
```

### Factory Pattern

RpcMethodRegistry creates method handlers:

```cpp
registry.registerMethod("getData",
    [](const QVector<RpcValue>& params) {
        return URCommonFunctions::getData(params);
    });
```

## Configuration

Settings stored in JSON format:

```json
{
  "info": { "version": "2.0.0-beta", "UR_Model": "UR10" },
  "robot": { "ip": "192.168.0.1" },
  "server": { "port": 8080 },
  "audio": { "enabled": true, "volume": 0.8 }
}
```

## Error Handling

- Exceptions for critical errors
- Return codes for expected failures
- Qt signals for async error notification
- Logging for diagnostics

## Performance Considerations

- Lazy loading of heavy modules
- Connection pooling for database
- Event-driven architecture (no polling loops)
- Efficient Qt containers (QVector, QString)
