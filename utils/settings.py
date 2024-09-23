# Description: This file contains the Settings class which is used to manage the settings of the application.

import tomli
import tomli_w
import os
import subprocess
import pickle
import base64
import copy

class Settings:
    def __init__(self):
        self.settings_file ='MultipackParser.conf'
        print(f"Initializing settings with file: {self.settings_file}")  # Debug statement
        self.settings = {
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
                    "debug": False
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
        self.saved_settings = None
        if not os.path.exists(self.settings_file):
            self.save_settings()
        else: 
            self.load_settings()
        self.saved_settings = copy.deepcopy(self.settings)
    
    def getDisplayModel(self):
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
        print(f"Saving settings to {self.settings_file=}")  # Debug statement
        if isinstance(self.settings_file, str) and self.settings_file.strip() != "":
            encoded_settings = base64.b64encode(pickle.dumps(self.settings))
            with open(self.settings_file, 'wb') as file:
                file.write(encoded_settings)
            self.saved_settings = copy.deepcopy(self.settings)
        else:
            print("Invalid settings file path detected in save_settings()")  # Debug statement
            raise ValueError("Invalid settings file path")
            
    def load_settings(self):
        print(f"Loading settings from {self.settings_file}")  # Debug statement
        if self.settings_file and isinstance(self.settings_file, str) and self.settings_file.strip() != "":
            try:
                with open(self.settings_file, 'rb') as file:
                    encoded_settings = file.read()
                    self.settings = pickle.loads(base64.b64decode(encoded_settings))
            except Exception as e:
                raise RuntimeError(f"Failed to load settings from {self.settings_file}") from e
        else:
            print("Invalid settings file path detected in load_settings()")  # Debug statement
            raise ValueError("Invalid settings file path")

    def __str__(self):
        return str(self.settings)

    def compare_loaded_settings_to_saved_settings(self):
        print(f"Comparing loaded settings to saved settings...")  # Debug statement
        if self.settings != self.saved_settings:
           raise ValueError("Loaded settings do not match saved settings.")
           return False
        else:
            return True

    def reset_unsaved_changes(self):
        print(f"Resetting unsaved changes...")  # Debug statement
        self.settings = copy.deepcopy(self.saved_settings)

if __name__ == '__main__':
    print("Hell no you can't run this file directly!")