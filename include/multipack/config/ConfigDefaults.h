/**
 * @file ConfigDefaults.h
 * @brief Default configuration values
 *
 * Defines all default values for application settings.
 */

#ifndef MULTIPACK_CONFIG_CONFIGDEFAULTS_H
#define MULTIPACK_CONFIG_CONFIGDEFAULTS_H

#include <QString>

namespace multipack {
namespace config {

/**
 * @namespace Defaults
 * @brief Default configuration values
 */
namespace Defaults {

// Application info
constexpr const char* APP_NAME = "MultipackParser";
constexpr const char* VERSION = "1.7.9";
constexpr const char* ORGANIZATION = "Multipack";

// Robot defaults
constexpr const char* ROBOT_IP = "192.168.0.1";
constexpr int DASHBOARD_PORT = 29999;
constexpr int XMLRPC_PORT = 8080;
constexpr bool XMLRPC_AUTO_START = true;

// Database
constexpr const char* DATABASE_NAME = "paletten.db";

// USB
constexpr const char* USB_PATH_LINUX = "/media/usb0";
constexpr const char* USB_PATH_DEV = "..";  // Development mode

// Audio
constexpr bool AUDIO_ENABLED = true;
constexpr float AUDIO_VOLUME = 0.8f;

// Logging
constexpr const char* LOG_DIR = "logs";
constexpr int LOG_MAX_FILES = 10;

// Timeouts
constexpr int ROBOT_CONNECT_TIMEOUT_MS = 5000;
constexpr int ROBOT_POLL_INTERVAL_MS = 1000;
constexpr int XMLRPC_REQUEST_TIMEOUT_MS = 10000;

// UI / Display
constexpr bool VIRTUAL_KEYBOARD_ENABLED = true;
constexpr bool FULLSCREEN_DEFAULT = true;
constexpr int DISPLAY_WIDTH = 800;
constexpr int DISPLAY_HEIGHT = 600;

// Audio files
constexpr const char* ALARM_SOUND_FILE = "Sound/output.wav";
constexpr const char* SCANNER_WARNING_SOUND_FILE = "Sound/stepback.wav";

// Password
constexpr int PASSWORD_MIN_LENGTH = 4;

/**
 * @brief Get default USB path for current platform
 * @return Platform-specific USB path
 */
inline QString defaultUsbPath()
{
#ifdef Q_OS_WIN
    return "..";  // Development mode on Windows
#else
    return USB_PATH_LINUX;
#endif
}

/**
 * @brief Get default database path
 * @return Database file path
 */
inline QString defaultDatabasePath()
{
    return DATABASE_NAME;
}

/**
 * @brief Get default settings file path
 * @return Settings file path
 */
inline QString defaultSettingsPath()
{
    return "settings.json";
}

/**
 * @brief Get default log directory
 * @return Log directory path
 */
inline QString defaultLogDir()
{
    return LOG_DIR;
}

} // namespace Defaults

/**
 * @namespace Keys
 * @brief Setting key constants
 */
namespace Keys {

// Sections
constexpr const char* INFO = "info";
constexpr const char* ROBOT = "robot";
constexpr const char* DATABASE = "database";
constexpr const char* AUDIO = "audio";
constexpr const char* UI = "ui";
constexpr const char* ADMIN = "admin";

// Info section
constexpr const char* INFO_VERSION = "info.version";
constexpr const char* INFO_UR_MODEL = "info.UR_Model";
constexpr const char* INFO_UR_SERIAL_NUMBER = "info.UR_Serial_Number";
constexpr const char* INFO_UR_MANUFACTURING_DATE = "info.UR_Manufacturing_Date";
constexpr const char* INFO_UR_SOFTWARE_VERSION = "info.UR_Software_Version";
constexpr const char* INFO_PALLETTIERER_NAME = "info.Pallettierer_Name";
constexpr const char* INFO_PALLETTIERER_STANDORT = "info.Pallettierer_Standort";
constexpr const char* INFO_NUMBER_OF_PLANS = "info.number_of_plans";
constexpr const char* INFO_NUMBER_OF_USE_CYCLES = "info.number_of_use_cycles";
constexpr const char* INFO_LAST_RESTART = "info.last_restart";

// Robot section
constexpr const char* ROBOT_IP = "robot.ip";
constexpr const char* ROBOT_DASHBOARD_PORT = "robot.dashboard_port";

// Database section
constexpr const char* DATABASE_PATH = "database.path";

// Audio section
constexpr const char* AUDIO_ENABLED = "audio.enabled";
constexpr const char* AUDIO_VOLUME = "audio.volume";

// Display section
constexpr const char* DISPLAY = "display";
constexpr const char* DISPLAY_WIDTH = "display.width";
constexpr const char* DISPLAY_HEIGHT = "display.height";
constexpr const char* DISPLAY_FULLSCREEN = "display.fullscreen";
constexpr const char* DISPLAY_SPECS_MODEL = "display.specs.model";
constexpr const char* DISPLAY_SPECS_WIDTH = "display.specs.width";
constexpr const char* DISPLAY_SPECS_HEIGHT = "display.specs.height";
constexpr const char* DISPLAY_SPECS_REFRESH_RATE = "display.specs.refresh_rate";

// UI section
constexpr const char* UI_FULLSCREEN = "ui.fullscreen";
constexpr const char* UI_VIRTUAL_KEYBOARD = "ui.virtual_keyboard";

// Admin section
constexpr const char* ADMIN_PASSWORD_HASH = "admin.password_hash";
constexpr const char* ADMIN_PATH = "admin.path";
constexpr const char* ADMIN_ALARM_SOUND_FILE = "admin.alarm_sound_file";
constexpr const char* ADMIN_SCANNER_WARNING_SOUND_FILE = "admin.scanner_warning_sound_file";
constexpr const char* ADMIN_USB_KEY = "admin.usb_key";
constexpr const char* ADMIN_USB_EXPECTED_VALUE = "admin.usb_expected_value";

// Server
constexpr const char* SERVER_PORT = "server.port";
constexpr const char* SERVER_USB_PATH = "server.usb_path";
constexpr const char* SERVER_AUTO_START = "server.auto_start";

} // namespace Keys

} // namespace config
} // namespace multipack

#endif // MULTIPACK_CONFIG_CONFIGDEFAULTS_H
