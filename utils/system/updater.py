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
import re
import time
import datetime
from pathlib import Path
from packaging import version
from PySide6.QtWidgets import QMessageBox, QProgressDialog, QApplication
from PySide6.QtCore import Qt, QTimer, QThread, Signal
from PySide6.QtGui import QPixmap

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
    """Get latest release info from GitHub with improved error handling."""
    try:
        logger.info("Fetching latest GitHub release information...")
        response = requests.get(
            "https://api.github.com/repos/Snupai/MultipackParser/releases/latest",
            timeout=10,  # Increased timeout
            headers={
                'User-Agent': 'MultipackParser-Updater',
                'Accept': 'application/vnd.github.v3+json'
            }
        )
        
        if response.status_code == 200:
            data = response.json()
            
            # Find the correct asset for the current platform
            target_asset_name = "MultipackParser"
            asset_url = None
            
            for asset in data.get('assets', []):
                if asset['name'] == target_asset_name:
                    asset_url = asset['browser_download_url']
                    break
            
            if asset_url:
                version_str = data['tag_name'].strip('v')
                logger.info(f"Found GitHub release: {version_str}")
                return {
                    'version': version_str,
                    'url': asset_url,
                    'name': data.get('name', ''),
                    'body': data.get('body', '')
                }
            else:
                logger.warning(f"No suitable asset found in GitHub release. Available assets: {[a['name'] for a in data.get('assets', [])]}")
        elif response.status_code == 403:
            logger.warning("GitHub API rate limit exceeded or access denied")
        elif response.status_code == 404:
            logger.warning("GitHub repository or releases not found")
        else:
            logger.warning(f"GitHub API returned status {response.status_code}: {response.text}")
            
    except requests.exceptions.Timeout:
        logger.warning("Timeout while checking GitHub releases")
    except requests.exceptions.ConnectionError:
        logger.warning("Connection error while checking GitHub releases")
    except requests.exceptions.RequestException as e:
        logger.warning(f"Request error while checking GitHub releases: {e}")
    except Exception as e:
        logger.error(f"Unexpected error checking GitHub release: {e}")
    
    return None

def download_file(url, progress_dialog):
    """Download a file with progress reporting and cancellation support."""
    try:
        logger.info(f"Starting download from: {url}")
        
        # Create a temporary file
        with tempfile.NamedTemporaryFile(delete=False) as tmp_file:
            # Start download with streaming
            response = requests.get(url, stream=True, timeout=30)
            response.raise_for_status()  # Raise exception for bad status codes
            
            total_size = int(response.headers.get('content-length', 0))
            block_size = 8192
            downloaded = 0
            
            logger.info(f"Download size: {total_size} bytes")
            
            # Update progress dialog
            if total_size > 0:
                progress_dialog.setLabelText(f"Downloading update... (0 MB / {total_size/1024/1024:.1f} MB)")
            else:
                progress_dialog.setLabelText("Downloading update...")
                
            for data in response.iter_content(block_size):
                # Check if user cancelled
                if progress_dialog.wasCanceled():
                    logger.info("Download cancelled by user")
                    os.unlink(tmp_file.name)  # Clean up partial download
                    return None
                    
                downloaded += len(data)
                tmp_file.write(data)
                
                if total_size > 0:
                    progress = int((downloaded / total_size) * 40) + 40  # 40-80% range
                    progress_dialog.setValue(progress)
                    progress_dialog.setLabelText(
                        f"Downloading update... ({downloaded/1024/1024:.1f} MB / {total_size/1024/1024:.1f} MB)"
                    )
                    
            logger.info(f"Download completed: {tmp_file.name}")
            return tmp_file.name
            
    except requests.exceptions.Timeout:
        logger.error("Download timed out")
    except requests.exceptions.ConnectionError:
        logger.error("Connection error during download")
    except requests.exceptions.HTTPError as e:
        logger.error(f"HTTP error during download: {e}")
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
    """Verify that a file is a valid executable (Linux only)."""
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
        if file_size < 100000:  # Less than 100KB
            logger.error(f"File too small to be a valid binary: {file_size} bytes")
            return False, "File too small to be a valid binary"
        
        # Ensure executable permissions
        try:
            current_mode = os.stat(file_path).st_mode
            if not (current_mode & stat.S_IXUSR):
                logger.info(f"Making file executable: {file_path}")
                os.chmod(file_path, current_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
        except Exception as e:
            logger.warning(f"Could not set executable permissions: {e}")
            return False, f"Could not set executable permissions: {e}"
        
        # Try to get version from the executable
        try:
            result = subprocess.run(
                [file_path, "--version"],
                capture_output=True,
                text=True,
                timeout=15
            )
            
            if result.returncode == 0:
                version_output = (result.stdout + result.stderr).strip()
                logger.info(f"Version output from {file_path}: {version_output}")
                return True, version_output
            else:
                logger.error(f"Version check failed with return code {result.returncode}")
                logger.error(f"stderr: {result.stderr}")
                return False, f"Version check failed: {result.stderr}"
                
        except subprocess.TimeoutExpired:
            logger.error("Version check timed out")
            return False, "Version check timed out"
        except FileNotFoundError:
            logger.error(f"File not found or not executable: {file_path}")
            return False, "File not found or not executable"
        except Exception as e:
            logger.error(f"Error running version check: {e}")
            return False, f"Error running version check: {e}"
            
    except Exception as e:
        logger.error(f"Error verifying executable: {e}")
        return False, f"Error verifying executable: {e}"

def find_usb_drives():
    """Find all mounted USB drives on Linux (Raspberry Pi)."""
    usb_paths = []
    
    try:
        # Linux mount points for USB drives
        mount_points = [
            "/media",
            "/mnt", 
            "/run/media"
        ]
        
        # Also check current user's media directories
        username = os.getenv('USER')
        if username:
            mount_points.extend([
                f"/media/{username}",
                f"/run/media/{username}"
            ])
        
        for mount_point in mount_points:
            if os.path.exists(mount_point):
                try:
                    for item in os.listdir(mount_point):
                        full_path = os.path.join(mount_point, item)
                        # Check if it's a directory and potentially a mount point
                        if os.path.isdir(full_path) and not item.startswith('.'):
                            # Verify it's actually mounted by checking if it's accessible
                            try:
                                contents = os.listdir(full_path)
                                usb_paths.append(full_path)
                                logger.info(f"Found USB drive: {full_path} ({len(contents)} items)")
                            except (PermissionError, OSError):
                                logger.debug(f"Cannot access {full_path}, skipping")
                except PermissionError:
                    logger.warning(f"Permission denied accessing {mount_point}")
                except Exception as e:
                    logger.warning(f"Error scanning {mount_point}: {e}")
        
        logger.info(f"Total USB drives found: {len(usb_paths)}")
        return usb_paths
        
    except Exception as e:
        logger.error(f"Error finding USB drives: {e}")
        return []

def search_for_update_file(usb_paths):
    """Search for update file in USB drives, copying to local dir for checks."""
    update_file_names = ["MultipackParser", "multipackparser"]
    local_update_dir = os.path.expanduser("~/.HMI/update/")
    
    try:
        os.makedirs(local_update_dir, exist_ok=True)
    except Exception as e:
        logger.error(f"Cannot create local update directory: {e}")
        return None, None
    
    for usb_path in usb_paths:
        logger.info(f"Searching in USB drive: {usb_path}")
        try:
            # Walk through the directory tree with depth limit
            for root, dirs, files in os.walk(usb_path):
                # Limit depth to avoid going too deep
                level = root.replace(usb_path, '').count(os.sep)
                if level >= 3:  # Max 3 levels deep
                    dirs[:] = []  # Don't recurse further
                    continue
                    
                for file_name in files:
                    if file_name.lower() in [name.lower() for name in update_file_names]:
                        file_path = os.path.join(root, file_name)
                        logger.info(f"Found potential update file: {file_path}")
                        
                        # Copy to local dir for checks
                        local_path = os.path.join(local_update_dir, "MultipackParser")
                        
                        try:
                            shutil.copy2(file_path, local_path)
                            logger.info(f"Copied {file_path} to {local_path}")
                            
                            # Verify the file
                            is_valid, version_info = verify_executable_file(local_path)
                            if is_valid:
                                logger.info(f"Valid update file found: {local_path}")
                                return local_path, version_info
                            else:
                                logger.warning(f"Invalid update file {local_path}: {version_info}")
                                # Clean up invalid file
                                try:
                                    os.remove(local_path)
                                except:
                                    pass
                                    
                        except Exception as e:
                            logger.warning(f"Failed to copy update file to local dir: {e}")
                            continue
                            
        except PermissionError:
            logger.warning(f"Permission denied accessing {usb_path}")
        except Exception as e:
            logger.warning(f"Error searching {usb_path}: {e}")
    
    logger.info("No valid update file found on USB drives")
    return None, None

def extract_version_from_output(version_output):
    """Extract version number from version output."""
    if not version_output:
        return None
        
    # Clean up the output
    version_output = version_output.strip()
    
    # Try multiple patterns to extract version
    version_patterns = [
        r'Multipack Parser Application Version:\s*v?([\d.]+)',  # Exact format
        r'Version:\s*v?([\d.]+)',  # Generic version format  
        r'v?([\d]+\.[\d]+\.[\d]+)',  # Standard semver
        r'v?([\d]+\.[\d]+)',  # Major.minor
        r'([\d]+\.[\d]+\.[\d]+)',  # Just the numbers
    ]
    
    for pattern in version_patterns:
        match = re.search(pattern, version_output, re.IGNORECASE)
        if match:
            found_version = match.group(1)
            logger.info(f"Extracted version '{found_version}' using pattern '{pattern}'")
            return found_version
    
    logger.warning(f"Could not extract version from: {version_output}")
    return None

def get_current_binary_path():
    """Determine the current binary path on Linux."""
    # Try multiple possible binary paths for Linux
    possible_paths = [
        sys.executable if getattr(sys, 'frozen', False) else None,  # For PyInstaller
        sys.argv[0] if sys.argv else None,
        os.path.abspath(sys.argv[0]) if sys.argv else None,
        os.path.join(os.getcwd(), "MultipackParser"),
        "/usr/local/bin/MultipackParser",
        "/opt/MultipackParser/MultipackParser",
        os.path.expanduser("~/MultipackParser")
    ]
    
    for path in possible_paths:
        if path and os.path.exists(path) and os.path.isfile(path):
            logger.info(f"Found current binary at: {path}")
            return path
    
    raise Exception("Cannot determine current binary path")

def install_update(update_file, current_binary=None):
    """Install update with proper error handling and backup."""
    backup_file = None
    
    try:
        # Get current binary path
        if not current_binary or not os.path.exists(current_binary):
            current_binary = get_current_binary_path()
        
        logger.info(f"Installing update from {update_file} to {current_binary}")
        
        # Verify update file is valid
        is_valid, version_info = verify_executable_file(update_file)
        if not is_valid:
            raise Exception(f"Update file is not valid: {version_info}")
        
        # Create backup directory if it doesn't exist
        backup_dir = os.path.join(os.path.dirname(current_binary), 'backup')
        try:
            os.makedirs(backup_dir, exist_ok=True)
        except Exception as e:
            # If we can't create backup dir, use temp dir
            backup_dir = tempfile.gettempdir()
            logger.warning(f"Using temp dir for backup: {e}")
        
        # Create backup with timestamp
        timestamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
        backup_filename = f'MultipackParser_{timestamp}.backup'
        backup_file = os.path.join(backup_dir, backup_filename)
        
        try:
            shutil.copy2(current_binary, backup_file)
            logger.info(f"Created backup at {backup_file}")
        except Exception as e:
            logger.warning(f"Could not create backup: {e}")
            backup_file = None
        
        # Create update script that will run after app closes
        update_script_path = os.path.join(tempfile.gettempdir(), 'multipack_update.sh')
        
        # Create log file for update script debugging
        update_log_path = os.path.join(tempfile.gettempdir(), 'multipack_update.log')
        
        with open(update_script_path, 'w') as script:
            script.write(f'''#!/bin/bash
# MultipackParser Update Script

# Log file for debugging
LOG_FILE="{update_log_path}"
echo "$(date): Update script started" >> "$LOG_FILE"
echo "Update file: {update_file}" >> "$LOG_FILE"
echo "Current binary: {current_binary}" >> "$LOG_FILE"
echo "Backup file: {backup_file}" >> "$LOG_FILE"

# Wait for main process to fully exit
echo "$(date): Waiting 3 seconds for process to exit" >> "$LOG_FILE"
sleep 3

# Check if main process is still running
echo "$(date): Checking for running MultipackParser processes" >> "$LOG_FILE"
while pgrep -f "MultipackParser" > /dev/null; do
    echo "$(date): Waiting for MultipackParser to exit..." >> "$LOG_FILE"
    sleep 1
done
echo "$(date): No MultipackParser processes found" >> "$LOG_FILE"

# Replace the binary
echo "$(date): Attempting to replace binary" >> "$LOG_FILE"
cp "{update_file}" "{current_binary}"
if [ $? -eq 0 ]; then
    echo "$(date): Binary updated successfully" >> "$LOG_FILE"
    chmod +x "{current_binary}"
    echo "$(date): Made binary executable" >> "$LOG_FILE"
    
    # Clean up update file
    rm -f "{update_file}"
    echo "$(date): Cleaned up update file" >> "$LOG_FILE"
    
    # Reboot the system to ensure clean restart with new binary
    echo "$(date): Binary update completed successfully. Rebooting system to start new version." >> "$LOG_FILE"
    echo "$(date): System will reboot in 5 seconds..." >> "$LOG_FILE"
    
    # Give a moment for the log to be written
    sleep 2
    
    # Reboot the system
    sudo reboot
    
else
    echo "$(date): Failed to update binary" >> "$LOG_FILE"
    # Restore backup if available
    if [ -f "{backup_file}" ]; then
        cp "{backup_file}" "{current_binary}"
        echo "$(date): Restored from backup" >> "$LOG_FILE"
    fi
fi

echo "$(date): Update script completed" >> "$LOG_FILE"

# Clean up this script after a delay (keep log for debugging)
sleep 5
rm -f "{update_script_path}"
''')
        
        # Make script executable
        os.chmod(update_script_path, 0o755)
        
        # Show message to user about restart
        if hasattr(global_vars, 'main_window') and global_vars.main_window:
            QMessageBox.information(
                global_vars.main_window,
                "Update Ready",
                "The application will now close and update automatically.\n\n"
                "Please wait a few seconds for the application to restart with the new version."
            )
        
        # Start the update script in background
        subprocess.Popen(['/bin/bash', update_script_path], 
                        stdout=subprocess.DEVNULL, 
                        stderr=subprocess.DEVNULL)
        
        logger.info(f"Update script started: {update_script_path}")
        
        # Exit the application to allow update
        QTimer.singleShot(1000, exit_app)  # Exit after 1 second
        
        return True
        
    except Exception as e:
        logger.error(f"Error installing update: {e}")
        return False

def check_usb_update():
    """Check USB drives for MultipackParser updates and return path and version if newer."""
    try:
        logger.info("Checking USB drives for updates...")
        
        # Find USB drives
        usb_paths = find_usb_drives()
        if not usb_paths:
            logger.info("No USB drives found")
            return None, None
        
        # Search for update file in USB drives
        update_file_path, version_output = search_for_update_file(usb_paths)
        
        if not update_file_path or not version_output:
            logger.info("No valid update file found on USB drives")
            return None, None
        
        # Extract version from output
        found_version = extract_version_from_output(version_output)
        if not found_version:
            logger.warning("Could not extract version from update file")
            return None, None
        
        # Compare with current version
        current_version = get_current_version()
        logger.info(f"Current version: {current_version}, Found version: {found_version}")
        
        try:
            if version.parse(found_version) > version.parse(current_version):
                logger.info(f"Newer version found on USB: {found_version} > {current_version}")
                return update_file_path, found_version
            else:
                logger.info(f"USB version is not newer: {found_version} <= {current_version}")
                return None, None
        except Exception as e:
            logger.error(f"Error comparing versions: {e}")
            return None, None
            
    except Exception as e:
        logger.error(f"Error checking USB update: {e}")
        return None, None
    
    return None, None

class UpdateWorker(QThread):
    """Worker thread for update operations to prevent UI blocking."""
    
    # Signals for communicating with the main thread
    progress_updated = Signal(int, str)  # progress value, status text
    update_found = Signal(str, str, str)  # update_type, version, path_or_url
    no_updates_found = Signal(str)  # message
    error_occurred = Signal(str)  # error message
    update_completed = Signal()  # update installation completed
    
    def __init__(self):
        super().__init__()
        self.should_cancel = False
        
    def cancel(self):
        """Request cancellation of the update check."""
        self.should_cancel = True
        
    def run(self):
        """Main update checking logic running in worker thread."""
        try:
            current_version = get_current_version()
            logger.info(f"Current version: {current_version}")
            
            # STEP 1: Check USB/Local updates first (priority)
            self.progress_updated.emit(10, "Checking USB drives for updates...")
            
            if self.should_cancel:
                return
                
            update_path, new_version = check_usb_update()
            
            if update_path and new_version:
                self.progress_updated.emit(30, f"Found USB update: v{new_version}")
                self.update_found.emit("usb", new_version, update_path)
                return  # Let main thread handle user interaction
            else:
                self.progress_updated.emit(30, "No USB update found, checking online...")
                
            if self.should_cancel:
                return
                
            # STEP 2: Check online updates (fallback)
            if check_internet_connection():
                logger.info("Checking GitHub for updates...")
                self.progress_updated.emit(50, "Checking GitHub for updates...")
                
                if self.should_cancel:
                    return
                    
                # Get latest release info
                release_info = get_latest_github_release()
                if release_info and version.parse(release_info['version']) > version.parse(current_version):
                    logger.info(f"New version {release_info['version']} available online")
                    self.progress_updated.emit(60, f"Found online update: v{release_info['version']}")
                    self.update_found.emit("online", release_info['version'], release_info['url'])
                    return  # Let main thread handle user interaction
                else:
                    # No updates found anywhere
                    self.progress_updated.emit(100, "No updates available")
                    self.no_updates_found.emit(
                        f"You are running the latest version ({current_version}).\n\n"
                        "No updates found on USB drives or online."
                    )
            else:
                # No internet connection
                self.progress_updated.emit(100, "No internet connection")
                self.no_updates_found.emit(
                    f"You are running version {current_version}.\n\n"
                    "No USB updates found and no internet connection available for online updates."
                )
                
        except Exception as e:
            logger.error(f"Error during update check: {e}")
            self.error_occurred.emit(f"An error occurred while checking for updates:\n\n{str(e)}")

class UpdateDownloadWorker(QThread):
    """Worker thread for downloading updates."""
    
    progress_updated = Signal(int, str)  # progress value, status text
    download_completed = Signal(str)  # downloaded file path
    download_failed = Signal(str)  # error message
    
    def __init__(self, url):
        super().__init__()
        self.url = url
        self.should_cancel = False
        
    def cancel(self):
        """Request cancellation of the download."""
        self.should_cancel = True
        
    def run(self):
        """Download the update file."""
        try:
            logger.info(f"Starting download from: {self.url}")
            
            # Create a temporary file
            with tempfile.NamedTemporaryFile(delete=False) as tmp_file:
                # Start download with streaming
                response = requests.get(self.url, stream=True, timeout=30)
                response.raise_for_status()
                
                total_size = int(response.headers.get('content-length', 0))
                block_size = 8192
                downloaded = 0
                
                logger.info(f"Download size: {total_size} bytes")
                
                for data in response.iter_content(block_size):
                    if self.should_cancel:
                        logger.info("Download cancelled by user")
                        os.unlink(tmp_file.name)
                        return
                        
                    downloaded += len(data)
                    tmp_file.write(data)
                    
                    if total_size > 0:
                        progress = int((downloaded / total_size) * 40) + 40  # 40-80% range
                        self.progress_updated.emit(
                            progress,
                            f"Downloading update... ({downloaded/1024/1024:.1f} MB / {total_size/1024/1024:.1f} MB)"
                        )
                        
                logger.info(f"Download completed: {tmp_file.name}")
                self.download_completed.emit(tmp_file.name)
                
        except requests.exceptions.Timeout:
            self.download_failed.emit("Download timed out")
        except requests.exceptions.ConnectionError:
            self.download_failed.emit("Connection error during download")
        except requests.exceptions.HTTPError as e:
            self.download_failed.emit(f"HTTP error during download: {e}")
        except Exception as e:
            self.download_failed.emit(f"Error downloading update: {e}")

def check_for_updates():
    """Check for updates using a worker thread to prevent UI blocking."""
    # Disable main window during update check
    if global_vars.main_window:
        global_vars.main_window.setDisabled(True)
    
    # Create progress dialog
    progress = QProgressDialog("Checking for updates...", "Cancel", 0, 100, global_vars.main_window)
    progress.setWindowModality(Qt.WindowModal)
    progress.setAutoClose(False)
    progress.setValue(0)
    progress.show()
    
    # Create worker thread
    worker = UpdateWorker()
    download_worker = None
    
    def on_progress_updated(value, text):
        """Update progress dialog from worker thread."""
        progress.setValue(value)
        progress.setLabelText(text)
        
    def on_update_found(update_type, new_version, path_or_url):
        """Handle when an update is found."""
        nonlocal download_worker
        
        if update_type == "usb":
            # USB update found
            if QMessageBox.question(
                global_vars.main_window,
                "USB Update Available",
                f"Version {new_version} found on USB drive.\n\nInstall this update?",
                QMessageBox.Yes | QMessageBox.No,
                QMessageBox.Yes
            ) == QMessageBox.Yes:
                progress.setLabelText("Installing USB update...")
                progress.setValue(70)
                
                if install_update(path_or_url):
                    progress.setValue(100)
                    progress.setLabelText("Update installed successfully!")
                    QTimer.singleShot(1000, cleanup_and_exit)
                else:
                    cleanup_and_show_error("Failed to install USB update.\nPlease check the file and try again.")
            else:
                cleanup()
                
        elif update_type == "online":
            # Online update found
            if QMessageBox.question(
                global_vars.main_window,
                "Online Update Available",
                f"Version {new_version} is available for download.\n\nDownload and install?",
                QMessageBox.Yes | QMessageBox.No
            ) == QMessageBox.Yes:
                # Start download in separate thread
                download_worker = UpdateDownloadWorker(path_or_url)
                download_worker.progress_updated.connect(on_progress_updated)
                download_worker.download_completed.connect(on_download_completed)
                download_worker.download_failed.connect(on_download_failed)
                download_worker.start()
                
                # Update cancel button to cancel download
                def cancel_download():
                    if download_worker:
                        download_worker.cancel()
                    cleanup()
                progress.canceled.connect(cancel_download)
            else:
                cleanup()
                
    def on_download_completed(file_path):
        """Handle successful download completion."""
        progress.setValue(90)
        progress.setLabelText("Installing downloaded update...")
        
        if install_update(file_path):
            progress.setValue(100)
            progress.setLabelText("Update installed successfully!")
            QTimer.singleShot(1000, cleanup_and_exit)
        else:
            cleanup_and_show_error("Failed to install downloaded update.\nPlease try again later.")
            
    def on_download_failed(error_msg):
        """Handle download failure."""
        cleanup_and_show_error(f"Download failed: {error_msg}")
        
    def on_no_updates_found(message):
        """Handle when no updates are found."""
        progress.setValue(100)
        QMessageBox.information(
            global_vars.main_window,
            "No Updates Available",
            message
        )
        cleanup()
        
    def on_error_occurred(error_msg):
        """Handle errors during update check."""
        cleanup_and_show_error(error_msg)
        
    def cleanup():
        """Clean up resources and re-enable UI."""
        if progress:
            progress.close()
        if global_vars.main_window:
            global_vars.main_window.setDisabled(False)
            
    def cleanup_and_exit():
        """Clean up and exit for successful update."""
        cleanup()
        # Update process will handle restart
        
    def cleanup_and_show_error(error_msg):
        """Clean up and show error message."""
        cleanup()
        QMessageBox.critical(
            global_vars.main_window,
            "Update Error",
            error_msg
        )
        
    def on_cancel():
        """Handle cancel button press."""
        worker.cancel()
        if download_worker:
            download_worker.cancel()
        cleanup()
        
    # Connect signals
    worker.progress_updated.connect(on_progress_updated)
    worker.update_found.connect(on_update_found)
    worker.no_updates_found.connect(on_no_updates_found)
    worker.error_occurred.connect(on_error_occurred)
    progress.canceled.connect(on_cancel)
    
    # Start the worker thread
    worker.start()

