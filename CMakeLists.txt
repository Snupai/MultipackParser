cmake_minimum_required(VERSION 3.20)

set(MULTIPACK_DEFAULT_PROJECT_VERSION "2.0.0")
set(MULTIPACK_PROJECT_VERSION "${MULTIPACK_DEFAULT_PROJECT_VERSION}" CACHE STRING "Numeric package version for CMake/CPack")
if(NOT MULTIPACK_PROJECT_VERSION)
    set(MULTIPACK_PROJECT_VERSION "${MULTIPACK_DEFAULT_PROJECT_VERSION}" CACHE STRING "Numeric package version for CMake/CPack" FORCE)
endif()
project(MultipackParser VERSION ${MULTIPACK_PROJECT_VERSION} LANGUAGES CXX)

set(MULTIPACK_APP_VERSION "" CACHE STRING "Application version string embedded in the binary")
if(NOT MULTIPACK_APP_VERSION AND DEFINED ENV{MULTIPACK_APP_VERSION})
    set(MULTIPACK_APP_VERSION "$ENV{MULTIPACK_APP_VERSION}")
endif()
if(NOT MULTIPACK_APP_VERSION)
    find_package(Git QUIET)
    if(Git_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --tags --exact-match HEAD
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE MULTIPACK_GIT_DESCRIBE_RESULT
            OUTPUT_VARIABLE MULTIPACK_GIT_TAG
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(MULTIPACK_GIT_DESCRIBE_RESULT EQUAL 0 AND MULTIPACK_GIT_TAG)
            set(MULTIPACK_APP_VERSION "${MULTIPACK_GIT_TAG}")
        endif()
    endif()
endif()
if(NOT MULTIPACK_APP_VERSION)
    set(MULTIPACK_APP_VERSION "${PROJECT_VERSION}")
endif()
string(REGEX REPLACE "^refs/tags/" "" MULTIPACK_APP_VERSION "${MULTIPACK_APP_VERSION}")
string(REGEX REPLACE "^[vV]" "" MULTIPACK_APP_VERSION "${MULTIPACK_APP_VERSION}")
set(MULTIPACK_APP_VERSION "${MULTIPACK_APP_VERSION}" CACHE STRING "Application version string embedded in the binary" FORCE)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Options
option(BUILD_TESTS "Build test suite" OFF)
option(ENABLE_VTK "Enable VTK 3D visualization" ON)
option(STATIC_BUILD "Build static binary" OFF)
option(MULTIPACK_SAFE_ARM64_EMULATION "Disable ARM CPU tuning flags for QEMU cross-build stability" OFF)

# Include CMake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

# Find Qt6
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(QT_REQUIRED_COMPONENTS
    Core
    Widgets
    Network
    Sql
    Multimedia
    Concurrent
    UiTools
)

if(BUILD_TESTS)
    list(APPEND QT_REQUIRED_COMPONENTS Test)
endif()

find_package(Qt6 REQUIRED COMPONENTS ${QT_REQUIRED_COMPONENTS})
find_package(Threads REQUIRED)

# Find VTK for 3D visualization
if(ENABLE_VTK)
    find_package(VTK QUIET COMPONENTS
        CommonCore
        CommonDataModel
        RenderingCore
        RenderingOpenGL2
        InteractionStyle
        GUISupportQt
        IOImage
    )
    if(VTK_FOUND)
        add_definitions(-DENABLE_VTK)
        message(STATUS "VTK found: ${VTK_VERSION}")
    else()
        message(WARNING "VTK not found - 3D visualization will be disabled")
        set(ENABLE_VTK OFF)
    endif()
endif()

# Find SQLite3
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    message(STATUS "SQLite3 not found, using Qt SQL module")
endif()

# Find OpenSSL for cryptography
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    add_definitions(-DHAVE_OPENSSL)
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
else()
    message(WARNING "OpenSSL not found - password hashing will use Qt fallback")
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${Qt6Widgets_INCLUDE_DIRS}
)

if(SQLite3_FOUND)
    include_directories(${SQLite3_INCLUDE_DIRS})
endif()

if(OpenSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
endif()

# Compiler warnings and flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
endif()

# ARM64 specific flags
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    if(MULTIPACK_SAFE_ARM64_EMULATION)
        set(CMAKE_CXX_FLAGS_RELEASE "-O0 -DNDEBUG" CACHE STRING
            "Release flags for stable ARM64 Docker/QEMU builds" FORCE)
        message(STATUS "Building for ARM64 architecture (safe emulation flags enabled)")
    else()
        add_compile_options(-mcpu=cortex-a72 -mtune=cortex-a72)
        message(STATUS "Building for ARM64 architecture")
    endif()
endif()

# Qt UI files location
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/ui)

# UI files
set(UI_FILES
    ${CMAKE_SOURCE_DIR}/ui/MainWindow.ui
    ${CMAKE_SOURCE_DIR}/ui/PasswordEntry.ui
)

# Resource files
qt6_add_resources(RESOURCES
    ${CMAKE_SOURCE_DIR}/resources/multipack.qrc
    ${CMAKE_SOURCE_DIR}/ui/MainWindowResources.qrc
)

# Header files with Q_OBJECT (required for AUTOMOC)
set(MOC_HEADERS
    include/multipack/core/Application.h
    include/multipack/core/GlobalState.h
    include/multipack/core/AppInitializer.h
    include/multipack/ui/MainWindow.h
    include/multipack/ui/OnScreenKeyboard.h
    include/multipack/ui/PasswordDialog.h
    include/multipack/ui/BlinkingLabel.h
    include/multipack/ui/MessageDialog.h
    include/multipack/ui/NotificationPopup.h
    include/multipack/ui/InputValidation.h
    include/multipack/ui/VisualizationWidget.h
    include/multipack/ui/PaletteConfigDialog.h
    include/multipack/ui/SplashScreen.h
    include/multipack/database/DatabaseManager.h
    include/multipack/network/XmlRpcServer.h
    include/multipack/network/RpcMethodRegistry.h
    include/multipack/robot/RobotController.h
    include/multipack/robot/RobotStatusMonitor.h
    include/multipack/robot/DashboardClient.h
    include/multipack/audio/AudioManager.h
    include/multipack/audio/AudioQueue.h
    include/multipack/audio/SafetyMonitor.h
    include/multipack/message/MessageManager.h
    include/multipack/message/StatusManager.h
    include/multipack/config/SettingsManager.h
    include/multipack/system/UsbKeyCheck.h
    include/multipack/system/UpdateChecker.h
    include/multipack/system/AutoUpdater.h
    include/multipack/system/UsbMonitor.h
)

# Source files organization
set(CORE_SOURCES
    src/core/Application.cpp
    src/core/GlobalState.cpp
    src/core/AppInitializer.cpp
    src/core/ArgumentParser.cpp
)

set(MAIN_SOURCE
    src/main.cpp
)

set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/OnScreenKeyboard.cpp
    src/ui/PasswordDialog.cpp
    src/ui/VisualizationWidget.cpp
    src/ui/NotificationPopup.cpp
    src/ui/InputValidation.cpp
    src/ui/PaletteConfigDialog.cpp
    src/ui/BlinkingLabel.cpp
    src/ui/MessageDialog.cpp
    src/ui/UIHelpers.cpp
    src/ui/SplashScreen.cpp
)

set(DATABASE_SOURCES
    src/database/DatabaseManager.cpp
    src/database/PalletData.cpp
    src/database/DatabaseMigrations.cpp
    src/database/DatabaseModels.cpp
)

set(NETWORK_SOURCES
    src/network/XmlRpcServer.cpp
    src/network/UR10ServerFunctions.cpp
    src/network/UR20ServerFunctions.cpp
    src/network/URCommonFunctions.cpp
    src/network/RpcMethodRegistry.cpp
)

set(ROBOT_SOURCES
    src/robot/RobotController.cpp
    src/robot/RobotStatusMonitor.cpp
    src/robot/RobotEnums.cpp
    src/robot/DashboardClient.cpp
    src/robot/RobotCommands.cpp
)

set(AUDIO_SOURCES
    src/audio/AudioManager.cpp
    src/audio/AudioQueue.cpp
    src/audio/SafetyMonitor.cpp
)

set(MESSAGE_SOURCES
    src/message/MessageManager.cpp
    src/message/StatusManager.cpp
    src/message/Message.cpp
)

set(CONFIG_SOURCES
    src/config/SettingsManager.cpp
    src/config/LoggingConfig.cpp
    src/config/ConfigDefaults.cpp
)

set(SYSTEM_SOURCES
    src/system/UsbKeyCheck.cpp
    src/system/UpdateChecker.cpp
    src/system/FileOperations.cpp
    src/system/RobFileParser.cpp
    src/system/AutoUpdater.cpp
    src/system/UsbMonitor.cpp
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64" AND MULTIPACK_SAFE_ARM64_EMULATION)
    # GCC 11 under Docker/QEMU can ICE/segfault on these translation units at -O2.
    # Keep the workaround tightly scoped so the ARM64 release build stays fast.
    set_source_files_properties(
        src/system/AutoUpdater.cpp
        src/ui/UIHelpers.cpp
        PROPERTIES COMPILE_OPTIONS "-O1")
endif()

set(UTILS_SOURCES
    src/utils/StringUtils.cpp
    src/utils/SocketUtils.cpp
    src/utils/ValidationUtils.cpp
    src/utils/VersionUtils.cpp
)

# Combine all sources
set(LIBRARY_SOURCES
    ${MOC_HEADERS}
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${DATABASE_SOURCES}
    ${NETWORK_SOURCES}
    ${ROBOT_SOURCES}
    ${AUDIO_SOURCES}
    ${MESSAGE_SOURCES}
    ${CONFIG_SOURCES}
    ${SYSTEM_SOURCES}
    ${UTILS_SOURCES}
    ${RESOURCES}
    ${UI_FILES}
)

# Create reusable application library
add_library(multipack-lib STATIC ${LIBRARY_SOURCES})
target_compile_definitions(multipack-lib
    PUBLIC
        "MULTIPACK_APP_VERSION=\"${MULTIPACK_APP_VERSION}\""
)

target_link_libraries(multipack-lib
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        Qt6::Sql
        Qt6::Multimedia
        Qt6::Concurrent
        Threads::Threads
)

if(SQLite3_FOUND)
    target_link_libraries(multipack-lib PUBLIC SQLite::SQLite3)
endif()

if(OpenSSL_FOUND)
    target_link_libraries(multipack-lib
        PUBLIC
            OpenSSL::SSL
            OpenSSL::Crypto
    )
endif()

if(ENABLE_VTK AND VTK_FOUND)
    target_link_libraries(multipack-lib PUBLIC ${VTK_LIBRARIES})
    vtk_module_autoinit(
        TARGETS multipack-lib
        MODULES ${VTK_LIBRARIES}
    )
endif()

# Create executable
add_executable(multipack-parser ${MAIN_SOURCE})
target_link_libraries(multipack-parser PRIVATE multipack-lib)

# Set output directory and RPATH for bundled libraries
set_target_properties(multipack-parser PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    INSTALL_RPATH "$ORIGIN/lib:$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# Installation
install(TARGETS multipack-parser
    RUNTIME DESTINATION bin
)

# Install resources
install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources/
    DESTINATION share/multipack-parser/resources
    FILES_MATCHING
    PATTERN "*.png"
    PATTERN "*.ico"
    PATTERN "*.wav"
)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# CPack configuration for packaging
set(CPACK_PACKAGE_NAME "MultipackParser")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "Multipack")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Multipack Parser Application")
set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Yann-Luca Näher")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6widgets6, libsqlite3-0")
include(CPack)

# Print configuration summary
message(STATUS "")
message(STATUS "MultipackParser Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  App version: ${MULTIPACK_APP_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Qt6 version: ${Qt6_VERSION}")
message(STATUS "  VTK enabled: ${ENABLE_VTK}")
if(ENABLE_VTK AND VTK_FOUND)
    message(STATUS "  VTK version: ${VTK_VERSION}")
endif()
message(STATUS "  XML-RPC server: built-in")
message(STATUS "  OpenSSL support: ${OpenSSL_FOUND}")
message(STATUS "  Target architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")
