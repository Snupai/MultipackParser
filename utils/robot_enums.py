from enum import Enum, auto

class RobotMode(Enum):
    """Enum for robot operating modes"""
    UNKNOWN = auto()
    POWER_OFF = auto()
    IDLE = auto()
    RUNNING = auto()
    BACKDRIVE = auto()
    ERROR = auto()

    @classmethod
    def from_string(cls, mode_str: str) -> 'RobotMode':
        """Convert string response to RobotMode enum"""
        if not mode_str or "Error" in mode_str:
            return cls.UNKNOWN
        mode_str = mode_str.lower()
        if "power_off" in mode_str:
            return cls.POWER_OFF
        elif "idle" in mode_str:
            return cls.IDLE
        elif "running" in mode_str:
            return cls.RUNNING
        elif "backdrive" in mode_str:
            return cls.BACKDRIVE
        return cls.UNKNOWN

class SafetyStatus(Enum):
    """Enum for robot safety status"""
    UNKNOWN = auto()
    NORMAL = auto()
    REDUCED = auto()
    PROTECTIVE_STOP = auto()
    RECOVERY = auto()
    SAFEGUARD_STOP = auto()
    SYSTEM_EMERGENCY_STOP = auto()
    ROBOT_EMERGENCY_STOP = auto()
    EMERGENCY_STOP = auto()
    VIOLATION = auto()
    FAULT = auto()
    ERROR = auto()

    @classmethod
    def from_string(cls, status_str: str) -> 'SafetyStatus':
        """Convert string response to SafetyStatus enum"""
        if not status_str or "Error" in status_str:
            return cls.UNKNOWN
        status_str = status_str.lower()
        if "normal" in status_str:
            return cls.NORMAL
        elif "reduced" in status_str:
            return cls.REDUCED
        elif "protective_stop" in status_str:
            return cls.PROTECTIVE_STOP
        elif "recovery" in status_str:
            return cls.RECOVERY
        elif "safeguard_stop" in status_str:
            return cls.SAFEGUARD_STOP
        elif "system_emergency_stop" in status_str:
            return cls.SYSTEM_EMERGENCY_STOP
        elif "robot_emergency_stop" in status_str:
            return cls.ROBOT_EMERGENCY_STOP
        elif "emergency_stop" in status_str:
            return cls.EMERGENCY_STOP
        elif "violation" in status_str:
            return cls.VIOLATION
        elif "fault" in status_str:
            return cls.FAULT
        return cls.UNKNOWN

class ProgramState(Enum):
    """Enum for robot program state"""
    UNKNOWN = auto()
    STOPPED = auto()
    PLAYING = auto()
    PAUSED = auto()
    ERROR = auto()

    @classmethod
    def from_string(cls, state_str: str) -> 'ProgramState':
        """Convert string response to ProgramState enum"""
        if not state_str or "Error" in state_str:
            return cls.UNKNOWN
        state_str = state_str.lower()
        if "stopped" in state_str:
            return cls.STOPPED
        elif "playing" in state_str:
            return cls.PLAYING
        elif "paused" in state_str:
            return cls.PAUSED
        return cls.UNKNOWN 