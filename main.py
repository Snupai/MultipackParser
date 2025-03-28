__license__ = '''
    Multipack Parser Application - to parse the data from the Multipack Robot to an UR Robot
    Copyright (C) 2024  Yann-Luca Näher

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
'''

import os
from ui_files import MainWindowResources_rc
################################################################
# DONT REMOVE these imports
# This is needed for the virtual keyboard to work
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuick import QQuickView
################################################################
#TODO: if start palette is not empty ask the user to confirm if the user wants to continue anyways and ask for the current layer.
#TODO: Implement seemless palletizing with 2 pallets for UR20 robot.
import sys
import logging
import matplotlib

from utils import global_vars
from utils.app_control import init_settings
from utils.logging_config import setup_logger
from utils.app_initialization import parse_arguments, initialize_app, setup_initial_app_state
from utils.database import create_database
from utils.robot_control import update_database_from_usb
from utils.ui_setup import (initialize_main_window, setup_input_validation, connect_signal_handlers,
                          setup_password_handling, setup_components, start_background_tasks,
                          setup_window_handling)

# Configure matplotlib backend for 3d view of palettes
matplotlib.use('qtagg', force=True)
os.environ["QT_IM_MODULE"] = "qtvirtualkeyboard"

logger = global_vars.logger

def main():
    """Main function to run the application.

    Returns:
        int: The exit code of the application.
    """
    try:
        # Parse command line arguments
        args = parse_arguments()

        if args.version:
            print(f"Multipack Parser Application Version: {global_vars.VERSION}")
            return 0
        if args.license:
            print(__license__)
            return 0

        # Setup logging first with verbose flag if specified
        # This ensures logging is properly configured before any other initialization
        setup_logger(args.verbose)
        logger.debug(f"MultipackParser Application Version: {global_vars.VERSION}") if args.verbose else logger.info(f"MultipackParser Application Version: {global_vars.VERSION}")

        # Initialize application
        app, splash, progress, loading_label = initialize_app()
        
        # Update progress
        progress.setValue(15)
        loading_label.setText("Setting up application...")
        app.processEvents()
        
        # create database
        progress.setValue(20)
        loading_label.setText("Creating database...")
        app.processEvents()
        create_database()

        # Create main window
        progress.setValue(25)
        loading_label.setText("Creating main window...")
        app.processEvents()
        
        initialize_main_window()

        # Initialize settings
        progress.setValue(50)
        loading_label.setText("Loading settings...")
        app.processEvents()
        
        init_settings()
        setup_initial_app_state()
        
        # update database
        progress.setValue(60)
        loading_label.setText("Updating database...")
        app.processEvents()
        
        update_database_from_usb()

        # Setup UI components
        progress.setValue(75)
        loading_label.setText("Setting up application...")
        app.processEvents()
        
        setup_input_validation()
        setup_components()
        connect_signal_handlers()
        setup_password_handling()
        setup_window_handling()

        # Start background tasks
        progress.setValue(90)
        loading_label.setText("Starting background tasks...")
        app.processEvents()
        
        start_background_tasks()

        # Final setup
        progress.setValue(100)
        loading_label.setText("Starting application...")
        app.processEvents()

        # Hide splash and show main window
        splash.finish(global_vars.main_window)
        global_vars.main_window.show()
        
        return app.exec()

    except Exception as e:
        # Show error in splash screen before exiting
        if 'loading_label' in locals() and 'progress' in locals() and 'app' in locals():
            loading_label.setText("Error during startup!")
            progress.setStyleSheet("""
                QProgressBar {
                    border: 2px solid grey;
                    border-radius: 5px;
                    text-align: center;
                    background-color: #f0f0f0;
                }
                QProgressBar::chunk {
                    background-color: #ff0000;
                    width: 10px;
                    margin: 0.5px;
                }
            """)
            app.processEvents()
            import time
            time.sleep(2)  # Show error for 2 seconds
        
        logger.critical(f"Fatal error in main: {str(e)}", exc_info=True)
        return 1

if __name__ == "__main__":
    sys.exit(main())
