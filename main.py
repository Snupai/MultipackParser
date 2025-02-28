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

from ui_files import MainWindowResources_rc

#TODO: After starting the program, ask the user to confirm each palette if it is empty or not. and if it is not empty ask the user to confirm if the user wants to continue anyways and ask for the current layer.
#TODO: Implement option for UR10e or UR20 robot. If UR20 is selected robot will have 2 pallets. else only it is like the old code.
#TODO: Implement seemless palletizing with 2 pallets for UR20 robot.

import sys
import os
import time
import argparse
import threading
import logging
import hashlib
from datetime import datetime

# import the needed qml modules for the virtual keyboard to work
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuick import QQuickView
################################################################
from PySide6.QtWidgets import (QApplication, QMainWindow, QMessageBox, 
                              QLabel, QProgressBar, QSplashScreen)
from PySide6.QtCore import Qt, QLocale, QRegularExpression
from PySide6.QtGui import QPixmap, QPainter, QColor, QRegularExpressionValidator, QIntValidator

from ui_files.ui_main_window import Ui_Form
from ui_files.visualization_3d import initialize_3d_view, clear_canvas
from utils import global_vars
from utils.message_manager import MessageManager
from PySide6 import QtCore
from utils.startup_dialogs import show_palette_config_dialog
from utils.ui_helpers import (CustomDoubleValidator, update_status_label, handle_scanner_status, 
                             set_wordlist, open_page, Page, open_password_dialog, leave_settings_page, 
                             open_file, save_open_file, execute_command, open_folder_dialog, 
                             open_file_dialog, set_settings_line_edits)
from utils.app_control import (show_instant_splash, setup_logging, exception_handler, 
                              qt_message_handler, init_settings, restart_app, exit_app)
from utils.robot_control import (load_rob_files, display_selected_file, load, 
                                send_cmd_play, send_cmd_pause, send_cmd_stop)
from utils.server import server_thread, server_stop
from utils.audio import (spawn_play_stepback_warning_thread, kill_play_stepback_warning_thread, 
                        set_audio_volume, delay_warning_sound)
from utils.updater import check_for_updates

import matplotlib
matplotlib.use('qtagg', force=True)

logger = global_vars.logger

os.environ["QT_IM_MODULE"] = "qtvirtualkeyboard"

def main():
    """Main function to run the application.

    Returns:
        int: The exit code of the application.
    """
    # Initialize the application
    app = QApplication.instance() or QApplication(sys.argv)
    
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
    
    # Start with initial progress
    progress.setValue(5)
    loading_label.setText("Initializing...")
    app.processEvents()

    try:
        # Parse command line arguments
        progress.setValue(10)
        loading_label.setText("Parsing arguments...")
        app.processEvents()
        
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
        
        debug_group = parser.add_argument_group('Debug Options') 
        debug_group.add_argument(
            '-V', '--verbose',
            action='store_true',
            help='Enable verbose (debug) logging output'
        )
        
        args = parser.parse_args()

        if args.version:
            print(f"Multipack Parser Application Version: {global_vars.VERSION}")
            return
        if args.license:
            print(__license__)
            return

        # Setup logging and basic initialization
        progress.setValue(15)
        loading_label.setText("Setting up logging...")
        app.processEvents()
        
        setup_logging(args.verbose)
        logger.debug(f"MultipackParser Application Version: {global_vars.VERSION}")
        global_vars.message_manager = MessageManager()
        QLocale.setDefault(QLocale(QLocale.Language.German, QLocale.Country.Germany))
        sys.excepthook = exception_handler
        QtCore.qInstallMessageHandler(qt_message_handler)
        
        progress.setValue(25)
        loading_label.setText("Creating main window...")
        app.processEvents()

        # Initialize main window and UI
        main_window = QMainWindow()
        main_window.setWindowFlags(Qt.WindowType.FramelessWindowHint)
        global_vars.main_window = main_window
        global_vars.ui = Ui_Form()
        global_vars.ui.setupUi(main_window)
        global_vars.ui.stackedWidget.setCurrentIndex(0)
        global_vars.ui.tabWidget_2.setCurrentIndex(0)

        progress.setValue(50)
        loading_label.setText("Loading settings...")
        app.processEvents()

        # Initialize settings
        init_settings()
        
        # Set last restart and number of use cycles
        global_vars.settings.settings['info']['last_restart'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        global_vars.settings.settings['info']['number_of_use_cycles'] = str(int(global_vars.settings.settings['info']['number_of_use_cycles']) + 1)
        global_vars.settings.save_settings()

        progress.setValue(75)
        loading_label.setText("Setting up application...")
        app.processEvents()

        # Write initial message
        update_status_label("Kein Pallettenplan geladen", "black", False, block=True)

        # Show palette configuration dialog for UR20 robot
        if global_vars.settings.settings['info']['UR_Model'] == 'UR20':
            logger.info("UR20 robot detected, showing palette configuration dialog")
            show_palette_config_dialog(main_window)
        else:
            logger.info(f"Robot model is {global_vars.settings.settings['info']['UR_Model']}, skipping palette configuration")
            global_vars.UR20_active_palette = 0
            global_vars.UR20_palette1_empty = False
            global_vars.UR20_palette2_empty = False

        # Set up input validation
        regex = QRegularExpression(r"^[0-9\-_]*$")
        validator = QRegularExpressionValidator(regex)
        global_vars.ui.EingabePallettenplan.setValidator(validator)

        int_validator = QIntValidator()
        global_vars.ui.EingabeKartonhoehe.setValidator(int_validator)

        float_validator = CustomDoubleValidator()
        float_validator.setNotation(CustomDoubleValidator.Notation.StandardNotation)
        float_validator.setDecimals(2)
        global_vars.ui.EingabeKartonGewicht.setValidator(float_validator)

        # Initialize components
        set_wordlist()
        canvas = initialize_3d_view(global_vars.ui.MatplotLibCanvasFrame)
        load_rob_files()

        # Set up spinbox limits
        global_vars.ui.EingabeStartlage.setMinimum(1)
        global_vars.ui.EingabeStartlage.setMaximum(99)

        progress.setValue(90)
        loading_label.setText("Connecting signals...")
        app.processEvents()

        # Connect all signals
        global_vars.ui.EingabePallettenplan.returnPressed.connect(load)
        global_vars.ui.openExperimentalTab.clicked.connect(lambda: open_page(Page.EXPERIMENTAL_PAGE))
        global_vars.ui.ButtonZurueck_8.clicked.connect(lambda: open_page(Page.MAIN_PAGE))
        global_vars.ui.robFilesListWidget.itemClicked.connect(lambda item: display_selected_file(item))
        global_vars.ui.deselectRobFile.clicked.connect(lambda: clear_canvas(canvas))

        # Connect all buttons
        global_vars.ui.ButtonSettings.clicked.connect(open_password_dialog)
        global_vars.ui.LadePallettenplan.clicked.connect(load)
        global_vars.ui.ButtonOpenParameterRoboter.clicked.connect(lambda: open_page(Page.PARAMETER_PAGE))
        global_vars.ui.ButtonDatenSenden.clicked.connect(server_thread)
        global_vars.ui.startaudio.clicked.connect(spawn_play_stepback_warning_thread)
        global_vars.ui.stopaudio.clicked.connect(kill_play_stepback_warning_thread)
        global_vars.ui.pushButtonVolumeOnOff.clicked.connect(set_audio_volume)

        # Connect robot control buttons
        global_vars.ui.ButtonZurueck.clicked.connect(lambda: open_page(Page.MAIN_PAGE))
        global_vars.ui.ButtonRoboterStart.clicked.connect(send_cmd_play)
        global_vars.ui.ButtonRoboterPause.clicked.connect(send_cmd_pause)
        global_vars.ui.ButtonRoboterStop.clicked.connect(send_cmd_stop)
        global_vars.ui.ButtonStopRPCServer.clicked.connect(server_stop)
        global_vars.ui.ButtonZurueck_2.clicked.connect(lambda: open_page(Page.MAIN_PAGE))
        global_vars.ui.ButtonDatenSenden_2.clicked.connect(server_thread)

        # Connect settings page buttons
        global_vars.ui.ButtonZurueck_3.clicked.connect(leave_settings_page)
        global_vars.ui.pushButtonSpeichern.clicked.connect(global_vars.settings.save_settings)
        global_vars.ui.ButtonZurueck_4.clicked.connect(leave_settings_page)
        global_vars.ui.pushButtonSpeichern_2.clicked.connect(global_vars.settings.save_settings)
        global_vars.ui.ButtonZurueck_5.clicked.connect(leave_settings_page)
        global_vars.ui.pushButtonSpeichern_3.clicked.connect(save_open_file)
        global_vars.ui.pushButtonOpenFile.clicked.connect(open_file)
        global_vars.ui.ButtonZurueck_6.clicked.connect(leave_settings_page)
        global_vars.ui.pushButtonSpeichern_4.clicked.connect(global_vars.settings.save_settings)
        global_vars.ui.ButtonZurueck_7.clicked.connect(leave_settings_page)

        # Connect settings text changed signals
        global_vars.ui.lineEditDisplayHeight.textChanged.connect(
            lambda text: global_vars.settings.settings['display']['specs'].__setitem__('height', int(text)))
        global_vars.ui.lineEditDisplayWidth.textChanged.connect(
            lambda text: global_vars.settings.settings['display']['specs'].__setitem__('width', int(text)))
        global_vars.ui.lineEditDisplayRefreshRate.textChanged.connect(
            lambda text: global_vars.settings.settings['display']['specs'].__setitem__('refresh_rate', int(text)))
        global_vars.ui.lineEditDisplayModel.textChanged.connect(
            lambda text: global_vars.settings.settings['display']['specs'].__setitem__('model', text))
        global_vars.ui.comboBoxChooseURModel.currentTextChanged.connect(
            lambda text: global_vars.settings.settings['info'].__setitem__('UR_Model', text))
        global_vars.ui.lineEditURSerialNo.textChanged.connect(
            lambda text: global_vars.settings.settings['info'].__setitem__('UR_Serial_Number', text))
        global_vars.ui.lineEditURManufacturingDate.textChanged.connect(
            lambda text: global_vars.settings.settings['info'].__setitem__('UR_Manufacturing_Date', text))
        global_vars.ui.lineEditURSoftwareVer.textChanged.connect(
            lambda text: global_vars.settings.settings['info'].__setitem__('UR_Software_Version', text))
        global_vars.ui.lineEditURName.textChanged.connect(
            lambda text: global_vars.settings.settings['info'].__setitem__('Pallettierer_Name', text))
        global_vars.ui.lineEditURStandort.textChanged.connect(
            lambda text: global_vars.settings.settings['info'].__setitem__('Pallettierer_Standort', text))
        global_vars.ui.lineEditNumberPlans.textChanged.connect(
            lambda text: global_vars.settings.settings['info'].__setitem__('number_of_plans', int(text)))
        global_vars.ui.lineEditNumberCycles.textChanged.connect(
            lambda text: global_vars.settings.settings['info'].__setitem__('number_of_use_cycles', int(text)))
        global_vars.ui.lineEditLastRestart.textChanged.connect(
            lambda text: global_vars.settings.settings['info'].__setitem__('last_restart', text))
        global_vars.ui.pathEdit.textChanged.connect(
            lambda text: global_vars.settings.settings['admin'].__setitem__('path', text))
        global_vars.ui.audioPathEdit.textChanged.connect(
            lambda text: global_vars.settings.settings['admin'].__setitem__('alarm_sound_file', text))

        # Connect file dialogs
        global_vars.ui.buttonSelectRobPath.clicked.connect(open_folder_dialog)
        global_vars.ui.buttonSelectAudioFilePath.clicked.connect(open_file_dialog)

        # Set up console
        global_vars.ui.lineEditCommand.setText("> ")
        global_vars.ui.lineEditCommand.setPlaceholderText("command")
        global_vars.ui.lineEditCommand.returnPressed.connect(execute_command)

        # Connect update and exit buttons
        global_vars.ui.pushButtonSearchUpdate.clicked.connect(check_for_updates)
        global_vars.ui.pushButtonExitApp.clicked.connect(restart_app)

        # Set up settings
        set_settings_line_edits()

        # Set up password handling
        def hash_password(password, salt=None):
            if salt is None:
                salt = os.urandom(16)
            salted_password = salt + password.encode()
            hashed_password = hashlib.sha256(salted_password).hexdigest()
            return salt.hex() + '$' + hashed_password

        global_vars.ui.passwordEdit.textChanged.connect(
            lambda text: global_vars.settings.settings['admin'].__setitem__('password', hash_password(text)) if text else None
        )

        # Set up window close handling
        global allow_close
        allow_close = False

        def allow_close_event(event):
            global allow_close
            if allow_close:
                event.accept()
                allow_close = False
            else:
                event.ignore()

        def handle_key_press_event(event):
            global allow_close
            if (event.modifiers() == (Qt.KeyboardModifier.ControlModifier | 
                                     Qt.KeyboardModifier.AltModifier | 
                                     Qt.KeyboardModifier.ShiftModifier) and 
                event.key() == Qt.Key.Key_C):
                allow_close = True
                exit_app()
            elif (event.modifiers() == (Qt.KeyboardModifier.ControlModifier | 
                                       Qt.KeyboardModifier.AltModifier) and 
                  event.key() == Qt.Key.Key_N):
                messageBox = QMessageBox()
                messageBox.setWindowTitle("Multipack Parser")
                messageBox.setTextFormat(Qt.TextFormat.RichText)
                messageBox.setText('''
                <div style="text-align: center;">
                Yann-Luca Näher - \u00a9 2024<br>
                <a href="https://github.com/Snupai">Github</a>
                </div>''')
                messageBox.setStandardButtons(QMessageBox.StandardButton.Ok)
                messageBox.setDefaultButton(QMessageBox.StandardButton.Ok)
                messageBox.exec()
                main_window.setWindowState(main_window.windowState() ^ Qt.WindowState.WindowActive)

        main_window.closeEvent = allow_close_event
        main_window.keyPressEvent = handle_key_press_event

        # Connect scanner signals
        from utils.UR20_Server_functions import scanner_signals
        scanner_signals.status_changed.connect(handle_scanner_status)

        # Start delay warning sound monitor thread
        warning_sound_thread = threading.Thread(target=delay_warning_sound)
        warning_sound_thread.daemon = True
        warning_sound_thread.start()

        # Final setup
        progress.setValue(100)
        loading_label.setText("Starting application...")
        app.processEvents()

        # Hide splash and show main window immediately
        splash.finish(main_window)
        main_window.show()
        
        sys.exit(app.exec())

    except Exception as e:
        # Show error in splash screen before exiting
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
        time.sleep(2)  # Show error for 2 seconds
        logger.critical(f"Fatal error in main: {str(e)}", exc_info=True)
        sys.exit(1)

if __name__ == "__main__":
    main()
