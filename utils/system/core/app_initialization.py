"""
Application initialization functions for the Multipack Parser app.
"""

import os
import sys
import logging
import argparse
from datetime import datetime
from PySide6.QtWidgets import QApplication, QSplashScreen, QProgressBar, QLabel, QMessageBox
from PySide6.QtCore import Qt, QLocale
from PySide6 import QtCore
from PySide6.QtGui import QPixmap, QPainter, QColor

from utils.system.core import global_vars
from utils.system.core.app_control import show_instant_splash, exception_handler, qt_message_handler
from utils.system.config.logging_config import setup_logger
from utils.ui.startup_dialogs import show_palette_config_dialog
from utils.message.message_manager import MessageManager
from utils.message.status_manager import update_status_label

# Add a logger for this module
logger = logging.getLogger(__name__)

def parse_arguments():
    """Parse command line arguments.
    
    Returns:
        argparse.Namespace: Parsed arguments
    """
    parser = argparse.ArgumentParser(
        description="Multipack Parser Application",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                     # Run normally
  %(prog)s -v                  # Show version and exit
  %(prog)s -V                  # Run with verbose logging
        """
    )
    
    info_group = parser.add_argument_group('Information')
    debug_group = parser.add_argument_group('Debug Options')
    debug_group.add_argument(
        '-V', '--verbose',
        action='store_true',
        help='Enable verbose (debug) logging output'
    )
    debug_group.add_argument(
        '--no-virtual-keyboard',
        action='store_true',
        help='Disable Qt virtual keyboard'
    )
    info_group.add_argument(
        '-v', '--version',
        action='store_true',
        help='Show version information and exit'
    )
    info_group.add_argument(
        '-l', '--license',
        action='store_true', 
        help='Show license information and exit'
    )
    
    return parser.parse_args()

def create_splash_screen(app):
    """Create and show the splash screen.
    
    Args:
        app: QApplication instance
        
    Returns:
        tuple: (splash, progress, loading_label) - The splash screen and its components
    """
    # Create and show the proper splash screen
    temp_splash = show_instant_splash()
    
    logo_pix = QPixmap(":/Szaidel Logo/imgs/logoszaidel-transparent-big.png")
    
    # Create a white background pixmap of the same size
    splash_pix = QPixmap(logo_pix.size())
    splash_pix.fill(QColor(255, 255, 255))  # Fill with white
    
    # Paint the logo onto the white background
    painter = QPainter(splash_pix)
    painter.drawPixmap(0, 0, logo_pix)
    painter.end()
    
    splash = QSplashScreen(splash_pix)
    
    # Add a progress bar to the splash screen
    progress = QProgressBar(splash)
    progress.setGeometry(splash_pix.width()/4, splash_pix.height() - 50, 
                        splash_pix.width()/2, 20)
    progress.setAlignment(Qt.AlignCenter)
    progress.setStyleSheet("""
        QProgressBar {
            border: 2px solid grey;
            border-radius: 5px;
            text-align: center;
            background-color: #f0f0f0;
        }
        QProgressBar::chunk {
            background-color:rgb(54, 71, 228);
            width: 10px;
            margin: 0.5px;
        }
    """)
    
    # Add loading text
    loading_label = QLabel(splash)
    loading_label.setGeometry(splash_pix.width()/4, splash_pix.height() - 80,
                            splash_pix.width()/2, 30)
    loading_label.setAlignment(Qt.AlignCenter)
    loading_label.setStyleSheet("color: #333333; font-size: 14px;")
    
    # Show new splash screen and hide temporary one
    splash.show()
    temp_splash.finish(splash)
    app.processEvents()
    
    return splash, progress, loading_label

def initialize_app():
    """Initialize the application basics.
    
    Returns:
        tuple: (app, splash, progress, loading_label) - The app and splash screen components
    """
    # Initialize the application
    app = QApplication.instance() or QApplication(sys.argv)
    
    # Create splash screen
    splash, progress, loading_label = create_splash_screen(app)
    
    # Start with initial progress
    progress.setValue(5)
    loading_label.setText("Initializing...")
    app.processEvents()
    
    # Set up basic app settings
    QLocale.setDefault(QLocale(QLocale.Language.German, QLocale.Country.Germany))
    sys.excepthook = exception_handler
    QtCore.qInstallMessageHandler(qt_message_handler)
    
    # Enable Qt virtual keyboard for all platforms
    os.environ["QT_IM_MODULE"] = "qtvirtualkeyboard"
    logger.info("Qt virtual keyboard enabled for all platforms")
    
    return app, splash, progress, loading_label

def setup_initial_app_state():
    """Set up the initial application state.
    
    This includes message manager, settings initialization and palette configuration.
    """
    # Initialize message manager
    global_vars.message_manager = MessageManager()
    
    # Set last restart and number of use cycles
    global_vars.settings.settings['info']['last_restart'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    global_vars.settings.settings['info']['number_of_use_cycles'] = str(int(global_vars.settings.settings['info']['number_of_use_cycles']) + 1)
    global_vars.settings.save_settings()

    # Write initial message
    update_status_label("Kein Pallettenplan geladen", "black", False, block=True)

    # Show palette configuration dialog for UR20 robot
    if global_vars.settings.settings['info']['UR_Model'] == 'UR20':
        logger.info("UR20 robot detected, showing palette configuration dialog")
        show_palette_config_dialog(global_vars.main_window)
    else:
        logger.info(f"Robot model is {global_vars.settings.settings['info']['UR_Model']}, skipping palette configuration")