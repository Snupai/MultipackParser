import socket
import time
import threading
from dataclasses import dataclass
from datetime import datetime
from typing import Optional, Tuple
from utils.system.core.global_vars import robot_ip, logger
from .robot_enums import RobotMode, SafetyStatus, ProgramState

# Constants
DASHBOARD_PORT = 29999
TIMEOUT_SECONDS = 5

@dataclass
class RobotStatus:
    """Data class to hold robot status information"""
    robot_mode: RobotMode = RobotMode.UNKNOWN
    safety_status: SafetyStatus = SafetyStatus.UNKNOWN
    program_state: ProgramState = ProgramState.UNKNOWN
    last_update: Optional[datetime] = None
    is_connected: bool = False
    connection_error: Optional[str] = None

def send_dashboard_command(command: str, ip: str = robot_ip, port: int = DASHBOARD_PORT, timeout: int = TIMEOUT_SECONDS) -> Tuple[str, bool, Optional[str]]:
    """
    Sends a command to the UR Dashboard Server and returns the response.
    
    Args:
        command: The command to send to the dashboard server
        ip: Robot IP address
        port: Dashboard server port
        timeout: Connection timeout in seconds
        
    Returns:
        Tuple[str, bool, Optional[str]]: 
            - Response from the dashboard server or error message
            - Success flag (True if command succeeded)
            - Error message if any
    """
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(timeout)
            s.connect((ip, port))
            welcome_message_bytes = s.recv(1024)
            s.sendall((command + "\n").encode("utf-8"))
            response_data = s.recv(1024)
            response = response_data.decode("utf-8").strip()
            
            # Check if response indicates robot is not powered on
            if "Robotmode: POWER_OFF" in response:
                return response, False, "Robot is powered off"
            elif "Error" in response:
                return response, False, response
            return response, True, None
            
    except socket.timeout:
        error_msg = f"Connection timeout to robot at {ip}:{port}"
        logger.error(error_msg)
        return "Error: Timeout", False, error_msg
    except ConnectionRefusedError:
        error_msg = f"Connection refused to robot at {ip}:{port} - Robot may be powered off"
        logger.error(error_msg)
        return "Error: Connection refused", False, error_msg
    except Exception as e:
        error_msg = f"Error sending dashboard command: {str(e)}"
        logger.error(error_msg)
        return f"Error: {str(e)}", False, error_msg

def get_polyscope_version() -> Tuple[str, bool, Optional[str]]:
    """
    Gets the Polyscope version from the robot.
    
    Returns:
        Tuple[str, bool, Optional[str]]:
            - Polyscope version or error message
            - Success flag (True if command succeeded)
            - Error message if any
    """
    response, success, error = send_dashboard_command("PolyscopeVersion")
    return response, success, error

class RobotStatusMonitor:
    def __init__(self, update_interval: int = 2):
        """
        Initialize the robot status monitor.
        
        Args:
            update_interval: Time between status updates in seconds
        """
        self.update_interval = update_interval
        self.running = False
        self.monitor_thread: Optional[threading.Thread] = None
        self.status = RobotStatus()
        
    def start_monitoring(self):
        """Start the status monitoring thread"""
        if not self.running:
            self.running = True
            self.monitor_thread = threading.Thread(target=self._monitor_loop, daemon=True)
            self.monitor_thread.start()
            logger.info("Robot status monitoring started")
            
    def stop_monitoring(self):
        """Stop the status monitoring thread"""
        self.running = False
        if self.monitor_thread:
            self.monitor_thread.join()
            logger.info("Robot status monitoring stopped")
            
    def _monitor_loop(self):
        """Main monitoring loop that updates robot status"""
        while self.running:
            try:
                # Get robot status
                robot_mode, mode_success, mode_error = send_dashboard_command("robotmode")
                safety_status, safety_success, safety_error = send_dashboard_command("safetystatus")
                program_state, prog_success, prog_error = send_dashboard_command("programstate")
                
                # Check if any command succeeded
                any_success = mode_success or safety_success or prog_success
                
                # Update status object with enum values
                self.status.robot_mode = RobotMode.from_string(robot_mode) if mode_success else RobotMode.UNKNOWN
                self.status.safety_status = SafetyStatus.from_string(safety_status) if safety_success else SafetyStatus.UNKNOWN
                self.status.program_state = ProgramState.from_string(program_state) if prog_success else ProgramState.UNKNOWN
                self.status.last_update = datetime.now()
                self.status.is_connected = any_success
                
                # Set connection error if all commands failed
                if not any_success:
                    error_messages = [msg for msg in [mode_error, safety_error, prog_error] if msg]
                    self.status.connection_error = error_messages[0] if error_messages else "Unknown connection error"
                else:
                    self.status.connection_error = None
                
                # Update global variables
                from .global_vars import current_robot_mode, current_safety_status, current_program_state
                current_robot_mode = self.status.robot_mode
                current_safety_status = self.status.safety_status
                current_program_state = self.status.program_state
                
            except Exception as e:
                logger.error(f"Error in status monitoring loop: {str(e)}")
                self.status.is_connected = False
                self.status.connection_error = str(e)
                
            time.sleep(self.update_interval)
            
    def get_current_status(self) -> RobotStatus:
        """
        Get the current robot status.
        
        Returns:
            RobotStatus: Current robot status object
        """
        return self.status 