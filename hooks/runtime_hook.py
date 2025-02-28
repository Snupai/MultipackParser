import os
import sys

# Initialize virtual keyboard behavior based on platform
is_arm_platform = False
if sys.platform.startswith('linux'):
    try:
        is_arm_platform = 'arm' in os.uname().machine
    except AttributeError:
        # os.uname() not available on all platforms
        import platform
        is_arm_platform = 'arm' in platform.machine().lower()

# Set QT_IM_MODULE for the right input method
if sys.platform.startswith('linux') and is_arm_platform:
    # For ARM Linux platforms, use compose for better compatibility
    os.environ["QT_IM_MODULE"] = "compose"
else:
    # For all other platforms, use the virtual keyboard
    os.environ["QT_IM_MODULE"] = "qtvirtualkeyboard"

# Ensure Qt plugins are found by setting the necessary paths
if getattr(sys, 'frozen', False):
    # Running as PyInstaller bundle
    bundle_dir = sys._MEIPASS
    os.environ["QT_PLUGIN_PATH"] = os.path.join(bundle_dir, "PySide6", "plugins")
    
    # Additional Qt environment variables that might help
    os.environ["QT_QPA_PLATFORM_PLUGIN_PATH"] = os.path.join(bundle_dir, "PySide6", "plugins", "platforms")
    os.environ["QML2_IMPORT_PATH"] = os.path.join(bundle_dir, "PySide6", "qml") 