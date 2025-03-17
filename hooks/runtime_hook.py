"""
Runtime hook for PyInstaller to ensure virtual keyboard functionality
"""
import os
import sys

# Set environment variable for QT virtual keyboard
os.environ["QT_IM_MODULE"] = "qtvirtualkeyboard"

# Inform the user about the environment setup
print("PyInstaller runtime hook: Setting QT_IM_MODULE=qtvirtualkeyboard")

# Ensure Qt plugins are found by setting the necessary paths
if getattr(sys, 'frozen', False):
    try:
        # Running as PyInstaller bundled app
        bundle_dir = sys._MEIPASS
        
        # Set up PySide6 plugin paths
        plugins_dir = os.path.join(bundle_dir, 'PySide6', 'plugins')
        platform_dir = os.path.join(plugins_dir, 'platforms')
        
        if os.path.exists(plugins_dir):
            os.environ['QT_PLUGIN_PATH'] = plugins_dir
            print(f"PyInstaller runtime hook: Setting QT_PLUGIN_PATH={plugins_dir}")
            
            # Check if we have virtual keyboard plugins
            vkb_dir = os.path.join(plugins_dir, 'virtualkeyboard')
            if os.path.exists(vkb_dir):
                print(f"Virtual keyboard plugins found at: {vkb_dir}")
            else:
                print("Warning: Virtual keyboard plugins not found")
        
        # Set QML import path
        qml_dir = os.path.join(bundle_dir, 'PySide6', 'qml')
        if os.path.exists(qml_dir):
            os.environ['QML2_IMPORT_PATH'] = qml_dir
            print(f"PyInstaller runtime hook: Setting QML2_IMPORT_PATH={qml_dir}")
            
        # Set platform plugin path
        if os.path.exists(platform_dir):
            os.environ['QT_QPA_PLATFORM_PLUGIN_PATH'] = platform_dir
            print(f"PyInstaller runtime hook: Setting QT_QPA_PLATFORM_PLUGIN_PATH={platform_dir}")
        
        # Additional possible locations to search for Qt plugins
        additional_plugin_dirs = [
            os.path.join(os.path.dirname(sys.executable), 'PySide6', 'plugins'),
            os.path.join(os.path.dirname(sys.executable), 'plugins'),
        ]
        
        for plugin_dir in additional_plugin_dirs:
            if os.path.exists(plugin_dir) and plugin_dir not in os.environ.get('QT_PLUGIN_PATH', ''):
                if 'QT_PLUGIN_PATH' in os.environ:
                    os.environ['QT_PLUGIN_PATH'] += os.pathsep + plugin_dir
                else:
                    os.environ['QT_PLUGIN_PATH'] = plugin_dir
                print(f"Added additional plugin path: {plugin_dir}")
    
    except Exception as e:
        print(f"Warning: Error setting up Qt paths: {e}") 