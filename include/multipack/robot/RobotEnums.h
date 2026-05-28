/**
 * @file RobotEnums.h
 */
#ifndef MULTIPACK_ROBOT_ROBOTENUMS_H
#define MULTIPACK_ROBOT_ROBOTENUMS_H
namespace multipack { namespace robot {
enum class RobotMode {
    Unknown, NoController, Disconnected, ConfirmSafety, Booting, PowerOff,
    PowerOn, Idle, BackDrive, Running
};
enum class SafetyStatus {
    Unknown, Normal, ReducedMode, ProtectiveStop, Recovery,
    SafeguardStop, SystemEmergencyStop, RobotEmergencyStop, Violation, Fault
};
enum class ProgramState {
    Unknown, Stopped, Playing, Paused
};
}} // namespace
#endif
