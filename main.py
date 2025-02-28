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
#TODO: After starting the program, ask the user to confirm each palette if it is empty or not. and if it is not empty ask the user to confirm if the user wants to continue anyways and ask for the current layer.
#TODO: Implement option for UR10e or UR20 robot. If UR20 is selected robot will have 2 pallets. else only it is like the old code.
#TODO: Implement seemless palletizing with 2 pallets for UR20 robot.
# The QT_IM_MODULE is now handled in initialize_app() function to ensure proper platform compatibility
import sys
import logging
import matplotlib

from utils import global_vars
from utils.app_control import setup_logging, init_settings
from utils.app_initialization import parse_arguments, initialize_app, setup_initial_app_state
from utils.ui_setup import (initialize_main_window, setup_input_validation, connect_signal_handlers,
                          setup_password_handling, setup_components, start_background_tasks,
                          setup_window_handling)

# Configure matplotlib backend
matplotlib.use('qtagg', force=True)

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

        # Initialize application
        app, splash, progress, loading_label = initialize_app()
        
        # Setup logging
        progress.setValue(15)
        loading_label.setText("Setting up logging...")
        app.processEvents()
        
        setup_logging(args.verbose)
        logger.debug(f"MultipackParser Application Version: {global_vars.VERSION}")

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
