import logging
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import requests
import urllib.request
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
        return global_vars.VERSION.split(" ")[-1]
    except:
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

def install_update(update_file, current_binary):
    """Install update with proper error handling and backup."""
    backup_file = None
    temp_update = None
    
    try:
        # Create backup directory if it doesn't exist
        backup_dir = os.path.join(os.path.dirname(current_binary), 'backup')
        os.makedirs(backup_dir, exist_ok=True)
        
        # Create backup with timestamp
        timestamp = subprocess.check_output(['date', '+%Y%m%d_%H%M%S']).decode().strip()
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
            
        # Create update script
        update_script = f"""#!/bin/bash
# Wait for parent process to exit
while ps -p $PPID > /dev/null; do
    sleep 1
done

# Replace binary
cp "{temp_update}" "{current_binary}"
chmod 755 "{current_binary}"

# Clean up
rm -f "{temp_update}"

# Restart application
"{current_binary}"
"""
        
        # Write update script to temporary file
        script_file = os.path.join(tempfile.gettempdir(), 'update_script.sh')
        with open(script_file, 'w') as f:
            f.write(update_script)
        os.chmod(script_file, 0o755)
        
        # Execute update script
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
    """Check USB drives for update file."""
    update_file_name = "MultipackParser"
    search_paths = ["/media", "/mnt"]
    
    for base_path in search_paths:
        if not os.path.exists(base_path):
            continue
            
        for root, _, files in os.walk(base_path):
            if update_file_name in files:
                update_path = os.path.join(root, update_file_name)
                try:
                    # Verify file is executable
                    os.chmod(update_path, 0o755)
                    if not os.access(update_path, os.X_OK):
                        continue
                        
                    # Get version
                    new_version = subprocess.check_output(
                        [update_path, "--version"],
                        stderr=subprocess.DEVNULL
                    ).decode().strip().split(" ")[-1]
                    
                    return update_path, new_version
                except:
                    continue
    
    return None, None

def check_for_updates():
    """Check for updates from GitHub or USB."""
    # Disable main window and show progress
    global_vars.main_window.setDisabled(True)
    progress = QProgressDialog("Checking for updates...", None, 0, 100, global_vars.main_window)
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
        
        # Check USB if no internet or GitHub update failed
        logger.info("Checking USB drives for updates...")
        progress.setLabelText("Checking USB drives...")
        progress.setValue(60)
        
        update_path, new_version = check_usb_update()
        if update_path and version.parse(new_version) > version.parse(current_version):
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
        elif not update_path:
            QMessageBox.information(
                None,
                "No Update Found",
                "No updates found on USB drives."
            )
    
    except Exception as e:
        logger.error(f"Error during update check: {e}")
        QMessageBox.critical(
            None,
            "Update Error",
            f"Error checking for updates: {str(e)}"
        )
    
    finally:
        progress.close()
        global_vars.main_window.setDisabled(False)