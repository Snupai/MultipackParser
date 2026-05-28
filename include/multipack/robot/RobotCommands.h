/**
 * @file RobotCommands.h
 * @brief Robot command definitions and utilities
 *
 * Defines all Dashboard Server commands and response parsing.
 */

#ifndef MULTIPACK_ROBOT_ROBOTCOMMANDS_H
#define MULTIPACK_ROBOT_ROBOTCOMMANDS_H

#include <QString>

namespace multipack {
namespace robot {

/**
 * @namespace Commands
 * @brief Dashboard Server command strings
 */
namespace Commands {
    // Status commands
    constexpr const char* ROBOT_MODE = "robotmode";
    constexpr const char* SAFETY_STATUS = "safetystatus";
    constexpr const char* PROGRAM_STATE = "programstate";
    constexpr const char* GET_LOADED_PROGRAM = "get loaded program";
    constexpr const char* IS_PROGRAM_SAVED = "isProgramSaved";
    constexpr const char* POPUP = "popup";

    // Program control
    constexpr const char* LOAD = "load ";  // + path
    constexpr const char* PLAY = "play";
    constexpr const char* PAUSE = "pause";
    constexpr const char* STOP = "stop";

    // Power control
    constexpr const char* POWER_ON = "power on";
    constexpr const char* POWER_OFF = "power off";
    constexpr const char* BRAKE_RELEASE = "brake release";
    constexpr const char* SHUTDOWN = "shutdown";

    // Safety
    constexpr const char* UNLOCK_PROTECTIVE_STOP = "unlock protective stop";
    constexpr const char* CLOSE_SAFETY_POPUP = "close safety popup";
    constexpr const char* RESTART_SAFETY = "restart safety";
    constexpr const char* CLOSE_POPUP = "close popup";

    // Info
    constexpr const char* VERSION = "version";
    constexpr const char* SERIAL_NUMBER = "get serial number";
    constexpr const char* ROBOT_MODEL = "get robot model";

    // Operational mode
    constexpr const char* GET_OPERATIONAL_MODE = "get operational mode";
    constexpr const char* SET_OPERATIONAL_MODE_MANUAL = "set operational mode manual";
    constexpr const char* SET_OPERATIONAL_MODE_AUTO = "set operational mode automatic";
    constexpr const char* CLEAR_OPERATIONAL_MODE = "clear operational mode";

    // Remote control
    constexpr const char* IS_IN_REMOTE_CONTROL = "is in remote control";
}

/**
 * @namespace ResponsePatterns
 * @brief Patterns for parsing Dashboard Server responses
 */
namespace ResponsePatterns {
    // Expected response prefixes
    constexpr const char* ROBOTMODE_PREFIX = "Robotmode:";
    constexpr const char* SAFETY_PREFIX = "Safetystatus:";
    constexpr const char* PROGRAM_PREFIX = "Program state:";
    constexpr const char* LOADED_PROGRAM_PREFIX = "Loaded program:";

    // Success responses
    constexpr const char* SUCCESS = "success";
    constexpr const char* TRUE_RESPONSE = "true";

    // Error responses
    constexpr const char* FAILED = "Failed";
    constexpr const char* ERROR_PREFIX = "Error:";
}

/**
 * @namespace CommandUtils
 * @brief Utility functions for command handling
 */
namespace CommandUtils {

/**
 * @brief Check if response indicates success
 * @param response Response string
 * @return true if success
 */
inline bool isSuccess(const QString& response)
{
    QString lower = response.toLower().trimmed();
    return lower.contains("success") ||
           lower.contains("true") ||
           lower.contains("starting");
}

/**
 * @brief Check if response indicates error
 * @param response Response string
 * @return true if error
 */
inline bool isError(const QString& response)
{
    QString lower = response.toLower().trimmed();
    return lower.contains("failed") ||
           lower.contains("error") ||
           lower.contains("false");
}

/**
 * @brief Extract value from response
 * @param response Full response
 * @param prefix Prefix to remove
 * @return Value portion
 */
inline QString extractValue(const QString& response, const QString& prefix)
{
    QString trimmed = response.trimmed();
    if (trimmed.startsWith(prefix, Qt::CaseInsensitive)) {
        return trimmed.mid(prefix.length()).trimmed();
    }
    return trimmed;
}

/**
 * @brief Build load command with path
 * @param path Program path
 * @return Complete load command
 */
inline QString loadCommand(const QString& path)
{
    return QString("%1%2").arg(Commands::LOAD).arg(path);
}

} // namespace CommandUtils

} // namespace robot
} // namespace multipack

#endif // MULTIPACK_ROBOT_ROBOTCOMMANDS_H
