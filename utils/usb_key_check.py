import os
from cryptography.fernet import Fernet
from utils import global_vars
import string
import ctypes

def check_hidden_key(usb_path):
    # First check for the index file
    settings = global_vars.settings
    
    index_file_path = os.path.join(usb_path, ".keyindex")
    if not os.path.exists(index_file_path):
        return False
    
    # Read the key file path from the index
    try:
        with open(index_file_path, 'r') as file:
            relative_key_path = file.read().strip()
        
        # Construct the absolute path for the current platform
        key_file_path = os.path.join(usb_path, relative_key_path)
        
        # Check if the key file exists
        if not os.path.exists(key_file_path):
            return False
            
        # Read the key file contents and verify it's a proper Fernet token
        with open(key_file_path, 'rb') as file:
            key_data = file.read()
            
            # Check if data exists
            if len(key_data) == 0:
                return False
                
            # Verify it's a valid Fernet token format and decrypts to expected value
            try:
                import base64
                # Try to decode as base64 (all Fernet tokens are base64 encoded)
                decoded = base64.urlsafe_b64decode(key_data)
                # Check minimum length for a valid Fernet token
                if len(decoded) < 32:
                    return False
                # Check version byte (should be 0x80 for Fernet)
                if decoded[0] != 0x80:
                    return False
                
                key: bytes = settings.settings['admin']['usb_key'].encode()
                # Try to decrypt and verify the content matches expected value
                fernet = Fernet(key)
                decrypted: bytes = fernet.decrypt(key_data)
                expected: bytes = settings.settings['admin']['usb_expected_value'].encode()
                return decrypted == expected
                
            except:
                return False
    except:
        return False

def check_any_usb_for_key():
    """
    Checks all connected USB drives for a valid security key.
    Returns True if a valid key is found on any drive, False otherwise.
    """
    # Get all available drives on Windows
    if os.name == 'nt':  # Windows
        drives = []
        bitmask = ctypes.windll.kernel32.GetLogicalDrives()
        for letter in string.ascii_uppercase:
            if bitmask & 1:
                drive_path = f"{letter}:\\"
                # Check if this is a removable drive (USB)
                drive_type = ctypes.windll.kernel32.GetDriveTypeW(drive_path)
                # 2 = DRIVE_REMOVABLE
                if drive_type == 2:
                    drives.append(drive_path)
            bitmask >>= 1
    else:  # Linux/macOS typically mount in /media or /mnt
        drives = []
        # Linux: check common mount points
        for base_path in ['/media', '/mnt']:
            if os.path.exists(base_path):
                # Get user name for Linux systems where drives mount under user
                user = os.getenv('USER')
                user_media = os.path.join(base_path, user) if user else base_path
                
                # Check both direct mounts and user-specific mounts
                for mount_path in [base_path, user_media]:
                    if os.path.exists(mount_path):
                        try:
                            for item in os.listdir(mount_path):
                                full_path = os.path.join(mount_path, item)
                                if os.path.ismount(full_path):
                                    drives.append(full_path)
                        except:
                            pass
    
    # Check each drive for a valid key
    for drive in drives:
        if check_hidden_key(drive):
            return True
    
    return False