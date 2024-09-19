# Description: This file contains the Settings class which is used to manage the settings of the application.

import tomli
import tomli_w
import os
import subprocess
import pickle

class Settings:
    def __init__(self, settings_file='MultipackParser.conf'):
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
                    "password": "666666",
                    "debug": False
                },
                "info": {
                    "number_of_plans": 0,
                    "number_of_use_cycles": 0,
                    "last_restart": "Never"
                }
        }

        if not os.path.exists(settings_file):
            self.save_settings(settings_file)
        else: 
            self.load_settings(settings_file)
    
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

    def save_settings(self, settings_file='MultipackParser.conf'):
        with open(settings_file, 'wb') as file:
            file.write(pickle.dumps(self.settings))

    def load_settings(self, settings_file='MultipackParser.conf'):
        try:
            with open(settings_file, 'rb') as file:
                self.settings = pickle.loads(file.read())
        except Exception as e:
            raise RuntimeError(f"Failed to load settings from {settings_file}") from e

    def __str__(self):
        return str(self.settings)

if __name__ == '__main__':
    print("Hell no you can't run this file directly!")