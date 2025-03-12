import os
import sys

# Enable Qt virtual keyboard for all platforms
os.environ["QT_IM_MODULE"] = "qtvirtualkeyboard"

# Ensure Qt plugins are found by setting the necessary paths
if getattr(sys, 'frozen', False):
    # Running as PyInstaller bundle
    bundle_dir = sys._MEIPASS
    os.environ["QT_PLUGIN_PATH"] = os.path.join(bundle_dir, "PySide6", "plugins")
    
    # Additional Qt environment variables that might help
    os.environ["QT_QPA_PLATFORM_PLUGIN_PATH"] = os.path.join(bundle_dir, "PySide6", "plugins", "platforms")
    os.environ["QML2_IMPORT_PATH"] = os.path.join(bundle_dir, "PySide6", "qml") 