import logging
import sys
import subprocess
import hashlib
import os
from PySide6.QtWidgets import QMessageBox
from PySide6.QtCore import Qt

from utils.system.core import global_vars
from utils.ui.ui_helpers import set_settings_line_edits
from utils.server.server import server_stop
from utils.system.config.settings import Settings

logger = logging.getLogger(__name__)

def restart_app():
    """Restart the application.
    """
    try:
        global_vars.settings.compare_loaded_settings_to_saved_settings()
    except ValueError as e:
        logger.error(f"Error: {e}")
        response = QMessageBox.question(global_vars.main_window, "Verwerfen oder Speichern", 
                                            "Möchten Sie die neuen Daten verwerfen oder speichern?",
                                            QMessageBox.StandardButton.Discard | 
                                            QMessageBox.StandardButton.Save, 
                                            QMessageBox.StandardButton.Save
                                            )
        if response == QMessageBox.StandardButton.Save:
            try:
                global_vars.settings.save_settings()
            except Exception as e:
                logger.error(f"Failed to save settings: {e}")
                return
    
    logger.info("Rebooting system...")
    if hasattr(global_vars, 'server') and global_vars.server:
        server_stop()
    subprocess.run(['sudo', 'reboot'], check=True)

def save_and_exit_app():
    """Save and exit the application.
    """
    try:
        global_vars.settings.compare_loaded_settings_to_saved_settings()
    except ValueError as e:
        logger.error(f"Error: {e}")
    
        # If settings do not match, ask whether to discard or save the new data
        response = QMessageBox.question(global_vars.main_window, "Verwerfen oder Speichern", 
                                            "Möchten Sie die neuen Daten verwerfen oder speichern?",
                                            QMessageBox.StandardButton.Discard | 
                                            QMessageBox.StandardButton.Save, 
                                            QMessageBox.StandardButton.Save)
        global_vars.main_window.setWindowState(global_vars.main_window.windowState() ^ Qt.WindowState.WindowActive)  # This will make the window blink
        if response == QMessageBox.StandardButton.Save:
            try:
                global_vars.settings.save_settings()
                logger.debug("New settings saved.")
            except Exception as e:
                logger.error(f"Failed to save settings: {e}")
                QMessageBox.critical(global_vars.main_window, "Error", f"Failed to save settings: {e}")
                return
        elif response == QMessageBox.StandardButton.Discard:
            global_vars.settings.reset_unsaved_changes()
            set_settings_line_edits()
            logger.debug("All changes discarded.")
    exit_app()

def exit_app():
    """Exit the application.
    """
    logger.info("Exiting application")
    
    # First stop the server if it's running
    if hasattr(global_vars, 'server') and global_vars.server:
        server_stop()
    
    # Stop any running audio threads
    if hasattr(global_vars, 'audio_thread_running') and global_vars.audio_thread_running:
        # from utils.audio.audio import kill_play_stepback_warning_thread
        # kill_play_stepback_warning_thread()
        pass
    
    # Use Qt's application exit mechanism for proper cleanup
    from PySide6.QtWidgets import QApplication
    app = QApplication.instance()
    if app:
        # Schedule the application to exit after processing current events
        app.exit(0)
    else:
        # Fallback to sys.exit if there's no Qt application instance
        sys.exit(0)

def init_settings():
    """Initialize the settings
    """
    settings = Settings()
    global_vars.settings = settings
    global_vars.PATH_USB_STICK = settings.settings['admin']['path']
    logger.debug(f"Settings initialized: {settings}")

def exception_handler(exc_type, exc_value, exc_traceback):
    """Global exception handler to log unhandled exceptions

    Args:
        exc_type (type): The type of the exception.
        exc_value (Exception): The exception value.
        exc_traceback (traceback): The traceback of the exception.
    """
    if issubclass(exc_type, KeyboardInterrupt):
        # Don't log keyboard interrupt
        sys.__excepthook__(exc_type, exc_value, exc_traceback)
        return
        
    logger.critical("Uncaught exception:", exc_info=(exc_type, exc_value, exc_traceback))

def qt_message_handler(mode, context, message):
    """Handler for Qt messages

    Args:
        mode (QtCore.QtMsgType): The mode of the message.
        context (QtCore.QtMsgType): The context of the message.
        message (str): The message to be logged.
    """
    from PySide6 import QtCore
    
    if mode == QtCore.QtMsgType.QtFatalMsg:
        logger.critical(f"Qt Fatal: {message}")
    elif mode == QtCore.QtMsgType.QtCriticalMsg:
        logger.critical(f"Qt Critical: {message}")
    elif mode == QtCore.QtMsgType.QtWarningMsg:
        logger.warning(f"Qt Warning: {message}")
    elif mode == QtCore.QtMsgType.QtInfoMsg:
        logger.info(f"Qt Info: {message}")

def show_instant_splash():
    """Show an instant splash screen before any initialization."""
    import sys
    from PySide6.QtWidgets import QApplication, QSplashScreen, QLabel
    from PySide6.QtGui import QPixmap, QPainter, QColor
    from PySide6.QtCore import Qt

    # Create minimal QApplication instance just for the splash
    if not QApplication.instance():
        temp_app = QApplication(sys.argv)
    else:
        temp_app = QApplication.instance()

    # Load the logo
    logo_pix = QPixmap(":/Szaidel Logo/imgs/logoszaidel-transparent-big.png")
    
    # Create a white background pixmap of the same size
    splash_pix = QPixmap(logo_pix.size())
    splash_pix.fill(QColor(255, 255, 255))  # Fill with white
    
    # Paint the logo onto the white background
    painter = QPainter(splash_pix)
    painter.drawPixmap(0, 0, logo_pix)
    painter.end()
    
    temp_splash = QSplashScreen(splash_pix)
    
    # Add a "Loading..." label
    loading_label = QLabel(temp_splash)
    loading_label.setGeometry(splash_pix.width()/4, splash_pix.height() - 50,
                            splash_pix.width()/2, 30)
    loading_label.setAlignment(Qt.AlignCenter)
    loading_label.setStyleSheet("color: #333333; font-size: 14px;")
    loading_label.setText("Loading...")
    
    # Show splash immediately
    temp_splash.show()
    temp_app.processEvents()
    
    return temp_splash 