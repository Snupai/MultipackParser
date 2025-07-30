import logging
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import requests
import urllib.request
import stat
import platform
from pathlib import Path
from packaging import version
from PySide6.QtWidgets import QMessageBox, QProgressDialog
from PySide6.QtCore import Qt

from utils.system.core import global_vars
from utils.system.core.app_control import exit_app

logger = logging.getLogger(__name__)

def check_internet_connection():
    """Check if we have internet connectivity by trying to reach common sites."""
    test_hosts = [
        ("8.8.8.8", 53),  # Google DNS
        ("1.1.1.1", 53),  # Cloudflare DNS
        ("github.com", 443)  # GitHub
    ]
    
    for host, port in test_hosts:
        try:
            socket.create_connection((host, port), timeout=3)
            return True
        except OSError:
            continue
    return False

def get_current_version():
    """Get current application version."""
    try:
        return global_vars.VERSION
    except AttributeError:
        return "0.0.0"

def get_latest_github_release():
    """Get latest release info from GitHub."""
    try:
        response = requests.get(
            "https://api.github.com/repos/Snupai/MultipackParser/releases/latest",
            timeout=5
        )
        if response.status_code == 200:
            data = response.json()
            return {
                'version': data['tag_name'].strip('v'),
                'url': next(
                    asset['browser_download_url'] 
                    for asset in data['assets'] 
                    if asset['name'] == 'MultipackParser'
                )
            }
    except Exception as e:
        logger.error(f"Error checking GitHub release: {e}")
    return None

def download_file(url, progress_dialog):
    """Download a file with progress reporting."""
    try:
        # Create a temporary file
        with tempfile.NamedTemporaryFile(delete=False) as tmp_file:
            # Download with progress
            response = requests.get(url, stream=True)
            total_size = int(response.headers.get('content-length', 0))
            block_size = 8192
            downloaded = 0
            
            for data in response.iter_content(block_size):
                downloaded += len(data)
                tmp_file.write(data)
                
                if total_size:
                    progress = int((downloaded / total_size) * 100)
                    progress_dialog.setValue(progress)
                    
            return tmp_file.name
    except Exception as e:
        logger.error(f"Error downloading update: {e}")
        return None

def get_system_info():
    """Get system information for compatibility checking."""
    return {
        'platform': platform.system(),
        'architecture': platform.machine(),
        'bits': platform.architecture()[0]
    }

def verify_executable_file(file_path):
    """Verify that a file is a valid executable."""
    try:
        # Check if file exists
        if not os.path.exists(file_path):
            logger.error(f"File does not exist: {file_path}")
            return False, "File does not exist"
        
        # Check if it's a regular file
        if not os.path.isfile(file_path):
            logger.error(f"Path is not a regular file: {file_path}")
            return False, "Path is not a regular file"
        
        # Check file size (should be reasonable for a binary)
        file_size = os.path.getsize(file_path)
        if file_size < 1000000:  # Less than 1MB
            logger.error(f"File too small to be a valid binary: {file_size} bytes")
            return False, "File too small to be a valid binary"
        
        # Check if file is executable
        if not os.access(file_path, os.X_OK):
            logger.info(f"File not executable, attempting to make executable: {file_path}")
            try:
                os.chmod(file_path, 0o755)
                if not os.access(file_path, os.X_OK):
                    logger.error(f"Failed to make file executable: {file_path}")
                    return False, "Failed to make file executable"
            except Exception as e:
                logger.error(f"Error making file executable: {e}")
                return False, f"Error making file executable: {e}"
        
        # Try to get version from the executable
        try:
            result = subprocess.run(
                [file_path, "--version"],
                capture_output=True,
                text=True,
                timeout=10
            )
            if result.returncode == 0:
                version_output = result.stdout.strip()
                logger.info(f"Version output: {version_output}")
                return True, version_output
            else:
                logger.error(f"Version check failed with return code {result.returncode}")
                return False, f"Version check failed: {result.stderr}"
        except subprocess.TimeoutExpired:
            logger.error("Version check timed out")
            return False, "Version check timed out"
        except Exception as e:
            logger.error(f"Error running version check: {e}")
            return False, f"Error running version check: {e}"
            
    except Exception as e:
        logger.error(f"Error verifying executable: {e}")
        return False, f"Error verifying executable: {e}"

def find_usb_drives():
    """Find all mounted USB drives."""
    usb_paths = []
    
    if platform.system() == "Windows":
        # Windows: Check for removable drives
        import string
        for drive_letter in string.ascii_uppercase:
            drive_path = f"{drive_letter}:\\"
            if os.path.exists(drive_path):
                try:
                    # Check if it's a removable drive
                    import ctypes
                    drive_type = ctypes.windll.kernel32.GetDriveTypeW(drive_path)
                    if drive_type == 2:  # DRIVE_REMOVABLE
                        usb_paths.append(drive_path)
                        logger.info(f"Found USB drive on Windows: {drive_path}")
                except Exception as e:
                    logger.warning(f"Error checking Windows drive {drive_path}: {e}")
    else:
        # Linux/macOS: Common mount points for USB drives
        mount_points = [
            "/media",
            "/mnt", 
            "/run/media",
            "/Volumes"  # macOS
        ]
        
        for mount_point in mount_points:
            if os.path.exists(mount_point):
                try:
                    for item in os.listdir(mount_point):
                        full_path = os.path.join(mount_point, item)
                        # Check if it's a directory and potentially a mount point
                        if os.path.isdir(full_path):
                            # Skip hidden directories
                            if not item.startswith('.'):
                                usb_paths.append(full_path)
                                logger.info(f"Found potential USB drive: {full_path}")
                except PermissionError:
                    logger.warning(f"Permission denied accessing {mount_point}")
                except Exception as e:
                    logger.warning(f"Error scanning {mount_point}: {e}")
    
    logger.info(f"Total USB drives found: {len(usb_paths)}")
    return usb_paths

def search_for_update_file(usb_paths):
    """Search for update file in USB drives, always copying to local dir for checks."""
    update_file_names = ["MultipackParser", "multipackparser"]
    local_update_dir = os.path.expanduser("~/.HMI/update/")
    os.makedirs(local_update_dir, exist_ok=True)
    
    for usb_path in usb_paths:
        logger.info(f"Searching in USB drive: {usb_path}")
        try:
            # Walk through the directory tree
            for root, dirs, files in os.walk(usb_path):
                for file_name in files:
                    if file_name in update_file_names:
                        file_path = os.path.join(root, file_name)
                        logger.info(f"Found potential update file: {file_path}")
                        # Always copy to local dir for checks
                        local_path = os.path.join(local_update_dir, file_name)
                        try:
                            shutil.copy2(file_path, local_path)
                            os.chmod(local_path, 0o755)
                        except Exception as e:
                            logger.warning(f"Failed to copy or chmod update file to local dir: {e}")
                            continue
                        # Verify the file (now local)
                        is_valid, version_info = verify_executable_file(local_path)
                        if is_valid:
                            logger.info(f"Valid update file found: {local_path}")
                            return local_path, version_info
                        else:
                            logger.warning(f"Invalid update file {local_path}: {version_info}")
        except PermissionError:
            logger.warning(f"Permission denied accessing {usb_path}")
        except Exception as e:
            logger.warning(f"Error searching {usb_path}: {e}")
    return None, None

def extract_version_from_output(version_output):
    """Extract version number from version output."""
    try:
        import re
        
        # Try multiple patterns to extract version
        version_patterns = [
            r'Multipack Parser Application Version:\s*([\d.]+)',  # Exact format
            r'Version:\s*([\d.]+)',  # Generic version format
            r'(\d+\.\d+\.\d+)',      # Any version number
        ]
        
        for pattern in version_patterns:
            match = re.search(pattern, version_output)
            if match:
                version = match.group(1)
                logger.info(f"Extracted version '{version}' using pattern '{pattern}'")
                return version
        
        logger.warning(f"Could not extract version from output: {version_output}")
        return None
    except Exception as e:
        logger.error(f"Error extracting version: {e}")
        return None

def install_update(update_file, current_binary):
    """Install update with proper error handling and backup."""
    backup_file = None
    temp_update = None
    
    try:
        # Get current binary path
        if not current_binary or not os.path.exists(current_binary):
            # Try multiple possible binary paths
            possible_paths = [
                sys.executable,
                sys.argv[0],
                os.path.abspath(sys.argv[0]),
                os.path.join(os.getcwd(), "MultipackParser"),
                "/usr/local/bin/MultipackParser",
                "/opt/MultipackParser/MultipackParser"
            ]
            
            for path in possible_paths:
                if path and os.path.exists(path):
                    current_binary = path
                    logger.info(f"Found current binary at: {current_binary}")
                    break
            else:
                raise Exception("Cannot determine current binary path")
        
        logger.info(f"Installing update from {update_file} to {current_binary}")
        
        # Create backup directory if it doesn't exist
        backup_dir = os.path.join(os.path.dirname(current_binary), 'backup')
        os.makedirs(backup_dir, exist_ok=True)
        
        # Create backup with timestamp
        import datetime
        timestamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
        backup_file = os.path.join(backup_dir, f'MultipackParser_{timestamp}.backup')
        shutil.copy2(current_binary, backup_file)
        logger.info(f"Created backup at {backup_file}")
        
        # Copy update to temporary location
        temp_update = current_binary + '.update'
        shutil.copy2(update_file, temp_update)
        
        # Make update executable
        os.chmod(temp_update, 0o755)
        
        # Verify update is executable
        if not os.access(temp_update, os.X_OK):
            raise Exception("Update file is not executable")
        
        # Test the update file
        test_result = subprocess.run(
            [temp_update, "--version"],
            capture_output=True,
            text=True,
            timeout=10
        )
        if test_result.returncode != 0:
            raise Exception(f"Update file test failed: {test_result.stderr}")
            
        # Create update script
        if platform.system() == "Windows":
            update_script = f"""@echo off
REM Wait for parent process to exit
:wait_loop
tasklist /FI "PID eq {os.getppid()}" 2>NUL | find /I /N "{os.getppid()}">NUL
if "%ERRORLEVEL%"=="0" (
    timeout /t 1 /nobreak >NUL
    goto wait_loop
)

REM Replace binary
copy "{temp_update}" "{current_binary}"
if errorlevel 1 (
    echo Failed to copy update file
    exit /b 1
)

REM Clean up
del "{temp_update}"

REM Restart application
start "" "{current_binary}"
"""
            script_file = os.path.join(tempfile.gettempdir(), 'update_script.bat')
            script_extension = '.bat'
        else:
            update_script = f"""#!/bin/bash
# Wait for parent process to exit
while ps -p $PPID > /dev/null; do
    sleep 1
done

# Replace binary
cp "{temp_update}" "{current_binary}"
if [ $? -ne 0 ]; then
    echo "Failed to copy update file"
    exit 1
fi

chmod 755 "{current_binary}"

# Clean up
rm -f "{temp_update}"

# Restart application
"{current_binary}"
"""
            script_file = os.path.join(tempfile.gettempdir(), 'update_script.sh')
            script_extension = '.sh'
        
        # Write update script to temporary file
        with open(script_file, 'w') as f:
            f.write(update_script)
        
        if platform.system() != "Windows":
            os.chmod(script_file, 0o755)
        
        # Execute update script
        if platform.system() == "Windows":
            subprocess.Popen(
                ['cmd', '/c', script_file],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                start_new_session=True
            )
        else:
            subprocess.Popen(
                ['/bin/bash', script_file],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                start_new_session=True
            )
        
        return True
        
    except Exception as e:
        logger.error(f"Error installing update: {e}")
        # Cleanup and restore from backup if needed
        if temp_update and os.path.exists(temp_update):
            try:
                os.remove(temp_update)
            except:
                pass
                
        if backup_file and os.path.exists(backup_file):
            try:
                shutil.copy2(backup_file, current_binary)
                logger.info("Restored from backup after failed update")
            except Exception as restore_error:
                logger.error(f"Failed to restore from backup: {restore_error}")
                
        return False

def check_usb_update():
    """Copy any MultipackParser from USB to ~/.HMI/update/, check version, update if newer."""
    import shutil
    import re
    import subprocess
    
    local_update_dir = os.path.expanduser("~/.HMI/update/")
    os.makedirs(local_update_dir, exist_ok=True)
    update_file_name = "MultipackParser"
    
    # Find USB drives
    usb_paths = find_usb_drives()
    if not usb_paths:
        return None, None
    
    # Search for update file in USB drives
    for usb_path in usb_paths:
        try:
            for root, dirs, files in os.walk(usb_path):
                for file_name in files:
                    if file_name == update_file_name:
                        src = os.path.join(root, file_name)
                        dst = os.path.join(local_update_dir, file_name)
                        
                        # Copy file to local directory
                        shutil.copy2(src, dst)
                        
                        # Make executable using sudo chmod +x
                        subprocess.run(['sudo', 'chmod', '+x', dst], check=True)
                        
                        # Check version
                        result = subprocess.run([dst, "--version"], capture_output=True, text=True, timeout=10)
                        if result.returncode == 0:
                            version_output = result.stdout.strip() + result.stderr.strip()
                            logger.info(f"Version output from {dst}: {version_output}")
                            
                            # Try multiple patterns to extract version
                            version_patterns = [
                                r'Multipack Parser Application Version:\s*([\d.]+)',  # Exact format
                                r'Version:\s*([\d.]+)',  # Generic version format
                                r'(\d+\.\d+\.\d+)',      # Any version number
                            ]
                            
                            found_version = None
                            for pattern in version_patterns:
                                match = re.search(pattern, version_output)
                                if match:
                                    found_version = match.group(1)
                                    logger.info(f"Extracted version '{found_version}' using pattern '{pattern}'")
                                    break
                            
                            if found_version:
                                current_version = get_current_version()
                                logger.info(f"Current version: {current_version}, Found version: {found_version}")
                                
                                from packaging import version
                                if version.parse(found_version) > version.parse(current_version):
                                    logger.info(f"Newer version found: {found_version} > {current_version}")
                                    return dst, found_version
                                else:
                                    logger.info(f"Found version is not newer: {found_version} <= {current_version}")
                            else:
                                logger.warning(f"Could not extract version from: {version_output}")
                        
                        return None, None
        except Exception as e:
            continue
    
    return None, None

def check_for_updates():
    """Check for updates from GitHub or USB with improved error handling."""
    # Disable main window and show progress
    if global_vars.main_window:
        global_vars.main_window.setDisabled(True)
    
    progress = QProgressDialog("Checking for updates...", None, 0, 100, global_vars.main_window if global_vars.main_window else None)
    progress.setWindowModality(Qt.WindowModal)
    progress.setAutoClose(True)
    progress.setValue(0)
    
    try:
        current_version = get_current_version()
        logger.info(f"Current version: {current_version}")
        
        # Check internet connection
        if check_internet_connection():
            logger.info("Checking GitHub for updates...")
            progress.setLabelText("Checking GitHub for updates...")
            progress.setValue(20)
            
            # Get latest release info
            release_info = get_latest_github_release()
            if release_info and version.parse(release_info['version']) > version.parse(current_version):
                logger.info(f"New version {release_info['version']} available")
                
                # Ask user to download
                if QMessageBox.question(
                    None, 
                    "Update Available",
                    f"Version {release_info['version']} is available. Download and install?",
                    QMessageBox.Yes | QMessageBox.No
                ) == QMessageBox.Yes:
                    
                    # Download update
                    progress.setLabelText("Downloading update...")
                    progress.setValue(40)
                    update_file = download_file(release_info['url'], progress)
                    
                    if update_file:
                        progress.setValue(80)
                        progress.setLabelText("Installing update...")
                        
                        # Install update
                        if install_update(update_file, sys.argv[0]):
                            progress.setValue(100)
                            QMessageBox.information(
                                None,
                                "Update Success",
                                "Update installed successfully.\nApplication will now restart."
                            )
                            exit_app()
                            return
                        else:
                            QMessageBox.critical(
                                None,
                                "Update Failed",
                                "Failed to install update.\nPlease try again later."
                            )
            else:
                QMessageBox.information(
                    None,
                    "No Update Available",
                    "You are running the latest version."
                )
        
        # Check USB for updates
        progress.setLabelText("Checking USB drives...")
        progress.setValue(60)
        
        update_path, new_version = check_usb_update()
        
        if update_path and new_version:
            if QMessageBox.question(
                None,
                "USB Update Available",
                f"Version {new_version} found on USB. Install?",
                QMessageBox.Yes | QMessageBox.No
            ) == QMessageBox.Yes:
                progress.setLabelText("Installing update...")
                progress.setValue(80)
                
                if install_update(update_path, sys.argv[0]):
                    progress.setValue(100)
                    QMessageBox.information(
                        None,
                        "Update Success",
                        "Update installed successfully.\nApplication will now restart."
                    )
                    exit_app()
                    return
                else:
                    QMessageBox.critical(
                        None,
                        "Update Failed",
                        "Failed to install update.\nPlease try again later."
                    )
    
    except Exception as e:
        logger.error(f"Error during update check: {e}")
        QMessageBox.critical(
            None,
            "Update Error",
            f"Error checking for updates:\n{str(e)}"
        )
    
    finally:
        progress.close()
        if global_vars.main_window:
            global_vars.main_window.setDisabled(False)

