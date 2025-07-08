# Description: This file contains the Settings class which is used to manage the settings of the application.

import os
import subprocess
import pickle
import base64
import copy
from PySide6.QtCore import QSettings
from .logging_config import logger

class Settings:
    """Settings class.
    """

    def __init__(self):
        """Initialize the settings with QSettings.
        """
        from utils.system.core import global_vars  # Import here to avoid circular import
        logger.debug("Initializing Settings")
        # Create QSettings instance with organization and application name
        self.qsettings = QSettings("Multipack", "MultipackParser")
        
        # Define default settings
        self.default_settings = {
            "display": {
                "width": 800,
                "height": 600,
                "fullscreen": False,
                "specs": {
                    "model": self.getDisplayModel(),
                    "width": self.getDisplayResolution()[0],
                    "height": self.getDisplayResolution()[1],
                    "refresh_rate": self.getDisplayRefreshRate()
                }
            },
            "audio": {
                "sound": True
            },
            "admin": {
                "password": "fc2f8726bb317b17a3cb322672818d2d$580c515fc8852dfd6e36faaaf46581c412683135b87dc8750c89efad4a38b54f",
                "path": "..",
                "alarm_sound_file": "Sound/output.wav",
                "scanner_warning_sound_file": "Sound/stepback.wav",
                "usb_key": "",
                "usb_expected_value": ""
            },
            "info": {
                "UR_Model": "N/A",
                "UR_Serial_Number": "N/A", 
                "UR_Manufacturing_Date": "N/A",
                "UR_Software_Version": "N/A",
                "Pallettierer_Name": "N/A",
                "Pallettierer_Standort": "N/A",
                "number_of_plans": 0,
                "number_of_use_cycles": 0,
                "last_restart": "Never"
            }
        }
        
        # Initialize settings if they don't exist
        self.settings = {}
        self.load_settings()

    def getDisplayModel(self):
        """Get the display model.

        Returns:
            str: The display model.
        """
        try:
            if os.name == 'nt':  # Windows
                result = subprocess.run(['wmic', 'desktopmonitor', 'get', 'caption'], capture_output=True, text=True)
            elif os.name == 'posix':  # Unix-like systems
                result = subprocess.run(['xrandr', '--verbose'], capture_output=True, text=True)
            else:
                return "Unknown"
            
            output = result.stdout
            return output.strip()
        except Exception as e:
            return f"Error retrieving display model: {str(e)}"

    def getDisplayResolution(self):
        """Get the display resolution.

        Returns:
            tuple: The display resolution as a tuple of width and height.
        """
        try:
            if os.name == 'nt':  # Windows
                result = subprocess.run(['wmic', 'path', 'Win32_VideoController', 'get', 'VideoModeDescription'], capture_output=True, text=True)
                lines = [line.strip() for line in result.stdout.split('\n') if line.strip()]
                if len(lines) > 1:
                    # Extract the resolution from the first data line after the header
                    resolution = lines[1].split('x')
                    width, height = resolution[0].strip(), resolution[1].strip()
                else:
                    return (800, 600)

            elif os.name == 'posix':  # Unix-like systems
                result = subprocess.run(['xrandr'], capture_output=True, text=True)
                for line in result.stdout.split('\n'):
                    if '*' in line:
                        resolution = line.split()[0]
                        width, height = resolution.split('x')
                        break
            else:
                return (800, 600)  # Default resolution if OS is unknown
            
            return int(width), int(height)
        except Exception as e:
            return (800, 600)  # Return a default resolution on error

    def getDisplayRefreshRate(self):
        """Get the display refresh rate.

        Returns:
            int: The display refresh rate.
        """
        try:
            if os.name == 'nt':  # Windows
                result = subprocess.run(['wmic', 'path', 'Win32_VideoController', 'get', 'CurrentRefreshRate'], capture_output=True, text=True)
                refresh_rate = int([x for x in result.stdout.split('\n') if x.strip()][1])  # Look for the non-empty line

            elif os.name == 'posix':  # Unix-like systems
                result = subprocess.run(['xrandr'], capture_output=True, text=True)
                for line in result.stdout.split('\n'):
                    if '*' in line:
                        refresh_rate = float(line.split()[1].replace('*', ''))
                        break
            else:
                return 60  # Default refresh rate if OS is unknown
            
            return refresh_rate
        except Exception as e:
            return 60  # Return a default refresh rate on error

    def save_settings(self):
        """Save settings using QSettings.
        """
        logger.debug("Saving settings")
        for group, values in self.settings.items():
            self.qsettings.beginGroup(group)
            for key, value in values.items():
                if isinstance(value, dict):
                    self.qsettings.beginGroup(key)
                    for subkey, subvalue in value.items():
                        self.qsettings.setValue(subkey, subvalue)
                    self.qsettings.endGroup()
                else:
                    self.qsettings.setValue(key, value)
            self.qsettings.endGroup()
        self.qsettings.sync()

    def load_settings(self):
        """Load settings from QSettings, using defaults if not found.
        """
        logger.debug("Loading settings")
        for group, values in self.default_settings.items():
            self.settings[group] = {}
            self.qsettings.beginGroup(group)
            for key, value in values.items():
                if isinstance(value, dict):
                    self.settings[group][key] = {}
                    self.qsettings.beginGroup(key)
                    for subkey, subvalue in value.items():
                        # Get the value and ensure correct type
                        loaded_value = self.qsettings.value(subkey, subvalue)
                        if isinstance(subvalue, bool):
                            # Convert string 'true'/'false' to bool
                            if isinstance(loaded_value, str):
                                loaded_value = loaded_value.lower() == 'true'
                            else:
                                loaded_value = bool(loaded_value)
                        elif isinstance(subvalue, int):
                            loaded_value = int(str(loaded_value))  # Convert to str first
                        elif isinstance(subvalue, float):
                            loaded_value = float(str(loaded_value))  # Convert to str first
                        self.settings[group][key][subkey] = loaded_value
                    self.qsettings.endGroup()
                else:
                    # Get the value and ensure correct type
                    loaded_value = self.qsettings.value(key, value)
                    if isinstance(value, bool):
                        # Convert string 'true'/'false' to bool
                        if isinstance(loaded_value, str):
                            loaded_value = loaded_value.lower() == 'true'
                        else:
                            loaded_value = bool(loaded_value)
                    elif isinstance(value, int):
                        loaded_value = int(str(loaded_value))  # Convert to str first
                    elif isinstance(value, float):
                        loaded_value = float(str(loaded_value))  # Convert to str first
                    self.settings[group][key] = loaded_value
            self.qsettings.endGroup()

    def reset_unsaved_changes(self):
        """Reset unsaved changes.
        """
        logger.debug("Resetting unsaved changes")
        self.load_settings()

    def compare_loaded_settings_to_saved_settings(self):
        """Compare current settings with those stored in QSettings.

        Raises:
            ValueError: If the loaded settings do not match the saved settings

        Returns:
            bool: True if the loaded settings match the saved settings, False otherwise
        """
        stored_settings = {}
        for group, values in self.default_settings.items():
            stored_settings[group] = {}
            self.qsettings.beginGroup(group)
            for key, value in values.items():
                if isinstance(value, dict):
                    stored_settings[group][key] = {}
                    self.qsettings.beginGroup(key)
                    for subkey, subvalue in value.items():
                        # Get the value and ensure correct type conversion
                        loaded_value = self.qsettings.value(subkey, subvalue)
                        if isinstance(subvalue, bool):
                            # Convert string 'true'/'false' to bool
                            if isinstance(loaded_value, str):
                                loaded_value = loaded_value.lower() == 'true'
                            else:
                                loaded_value = bool(loaded_value)
                        elif isinstance(subvalue, int):
                            loaded_value = int(str(loaded_value))  # Convert to str first
                        elif isinstance(subvalue, float):
                            loaded_value = float(str(loaded_value))  # Convert to str first
                        stored_settings[group][key][subkey] = loaded_value
                    self.qsettings.endGroup()
                else:
                    # Get the value and ensure correct type conversion
                    loaded_value = self.qsettings.value(key, value)
                    if isinstance(value, bool):
                        # Convert string 'true'/'false' to bool
                        if isinstance(loaded_value, str):
                            loaded_value = loaded_value.lower() == 'true'
                        else:
                            loaded_value = bool(loaded_value)
                    elif isinstance(value, int):
                        loaded_value = int(str(loaded_value))  # Convert to str first
                    elif isinstance(value, float):
                        loaded_value = float(str(loaded_value))  # Convert to str first
                    stored_settings[group][key] = loaded_value
            self.qsettings.endGroup()
        
        if stored_settings != self.settings:
            raise ValueError("Loaded settings do not match saved settings")
        return True

    def __str__(self):
        """String representation of settings.

        Returns:
            str: The string representation of the settings
        """
        return str(self.settings)

if __name__ == '__main__':
    print("Hell no you can't run this file directly!")