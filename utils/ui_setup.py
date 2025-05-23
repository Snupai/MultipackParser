"""
UI setup and signal connections for the Multipack Parser app.
"""

import os
import hashlib
import threading
import logging
from PySide6.QtWidgets import QMainWindow, QMessageBox
from PySide6.QtCore import Qt, QRegularExpression, QTimer
from PySide6.QtGui import QRegularExpressionValidator, QIntValidator

from ui_files.ui_main_window import Ui_Form
from ui_files.visualization_3d import initialize_3d_view, clear_canvas, load_rob_files
from utils import global_vars
from utils.status_manager import update_status_label
from utils.ui_helpers import (CustomDoubleValidator, handle_scanner_status,
                             set_wordlist, open_page, Page, open_password_dialog, leave_settings_page,
                             open_file, save_open_file, execute_command, open_folder_dialog, 
                             open_file_dialog, set_settings_line_edits, check_key_or_password, clear_filters,
                             show_palette_clear_dialog)
from utils.robot_control import (display_selected_file, load, 
                                send_cmd_play, send_cmd_pause, send_cmd_stop, load_selected_file,
                                send_remote_control_command)
from utils.server import server_thread, server_stop
from utils.audio import (spawn_play_stepback_warning_thread, kill_play_stepback_warning_thread, 
                        set_audio_volume, delay_warning_sound)
from utils.updater import check_for_updates
from utils.app_control import restart_app, exit_app

# Add logger
logger = logging.getLogger(__name__)

def initialize_main_window():
    """Initialize the main window and UI.
    
    Returns:
        QMainWindow: The initialized main window
    """
    main_window = QMainWindow()
    main_window.setWindowFlags(Qt.WindowType.FramelessWindowHint)
    global_vars.main_window = main_window
    global_vars.ui = Ui_Form()
    global_vars.ui.setupUi(main_window)
    global_vars.ui.stackedWidget.setCurrentIndex(0)
    global_vars.ui.tabWidget.setCurrentIndex(0)
    global_vars.ui.tabWidget_2.setCurrentIndex(0)
    
    return main_window

def setup_input_validation():
    """Set up input validation for UI elements."""
    # Set up input validation
    regex = QRegularExpression(r"^[0-9\-_]*$")
    validator = QRegularExpressionValidator(regex)
    global_vars.ui.EingabePallettenplan.setValidator(validator)
    
    # Force numeric keyboard with special number preference
    global_vars.ui.EingabePallettenplan.setInputMethodHints(
        Qt.ImhPreferNumbers |  # Prefer numeric keyboard
        Qt.ImhNoPredictiveText  # Disable predictive text
    )
    
    # Override event handling to prevent keyboard from closing
    original_focus_out = global_vars.ui.EingabePallettenplan.focusOutEvent
    
    def custom_focus_out(event):
        # Only process focus out if it's not going to the completer popup
        if global_vars.completer and global_vars.completer.popup().isVisible():
            event.ignore()  # Ignore focus out events when completer is visible
        else:
            original_focus_out(event)  # Process normal focus out events
            
    global_vars.ui.EingabePallettenplan.focusOutEvent = custom_focus_out

    int_validator = QIntValidator()
    global_vars.ui.EingabeKartonhoehe.setValidator(int_validator)
    global_vars.ui.EingabeKartonhoehe.setInputMethodHints(Qt.ImhDigitsOnly)

    float_validator = CustomDoubleValidator()
    float_validator.setNotation(CustomDoubleValidator.Notation.StandardNotation)
    float_validator.setDecimals(2)
    global_vars.ui.EingabeKartonGewicht.setValidator(float_validator)
    global_vars.ui.EingabeKartonGewicht.setInputMethodHints(Qt.ImhFormattedNumbersOnly)
    
    # Set up spinbox limits
    global_vars.ui.EingabeStartlage.setMinimum(1)
    global_vars.ui.EingabeStartlage.setMaximum(99)

def connect_signal_handlers():
    """Connect all signal handlers."""
    # set custom style to spinBoxes
    _customStyleSheet = """
            QSpinBox {
                padding-right: 40px;
            }
            QSpinBox::up-button {
                width: 39px;
                height: 39px;
                padding: 2px;
                subcontrol-position: right;
                subcontrol-origin: border;
            }
            QSpinBox::down-button {
                width: 39px;
                height: 39px;
                padding: 2px;
                subcontrol-position: right;
                subcontrol-origin: border;
                right: 39px;
            }
            QSpinBox::up-arrow {
                width: 12px;
                height: 12px;
            }
            QSpinBox::down-arrow {
                width: 12px;
                height: 12px;
            }
            """
    global_vars.ui.EingabeStartlage.setStyleSheet(_customStyleSheet)
    global_vars.ui.EingabeVerschiebungX.setStyleSheet(_customStyleSheet)
    global_vars.ui.EingabeVerschiebungY.setStyleSheet(_customStyleSheet)
        
    
    # Connect main UI signals
    global_vars.ui.EingabePallettenplan.returnPressed.connect(load)
    global_vars.ui.openExperimentalTab.clicked.connect(lambda: open_page(Page.EXPERIMENTAL_PAGE))
    global_vars.ui.ButtonZurueck_8.clicked.connect(lambda: open_page(Page.MAIN_PAGE))
    global_vars.ui.robFilesListWidget.itemClicked.connect(lambda item: display_selected_file(item))
    global_vars.ui.deselectRobFile.clicked.connect(lambda: clear_canvas(global_vars.canvas))
    global_vars.ui.LadePallettenplan_2.clicked.connect(load_selected_file)
    def update_filter_length(text_value):
        global_vars.filter_length = int(text_value) if text_value else 0
        load_rob_files()
    global_vars.ui.lineEditFilterLength.textChanged.connect(update_filter_length)
    def update_filter_width(text_value):
        global_vars.filter_width = int(text_value) if text_value else 0
        load_rob_files()
    global_vars.ui.lineEditFilterWidth.textChanged.connect(update_filter_width)
    def update_filter_height(text_value):
        global_vars.filter_height = int(text_value) if text_value else 0
        load_rob_files()
    global_vars.ui.lineEditFilterHeight.textChanged.connect(update_filter_height)
    global_vars.ui.pushButtonClearFilters.clicked.connect(clear_filters)

    # Connect all buttons
    global_vars.ui.ButtonSettings.clicked.connect(check_key_or_password)
    global_vars.ui.LadePallettenplan.clicked.connect(load)
    global_vars.ui.ButtonOpenParameterRoboter.clicked.connect(lambda: open_page(Page.PARAMETER_PAGE))
    global_vars.ui.ButtonDatenSenden.clicked.connect(server_thread)
    #global_vars.ui.startaudio.clicked.connect(spawn_play_stepback_warning_thread) # TODO: Temporarily disabled due to fast production push
    #global_vars.ui.stopaudio.clicked.connect(kill_play_stepback_warning_thread) # TODO: Temporarily disabled due to fast production push
    global_vars.ui.pushButtonVolumeOnOff.clicked.connect(set_audio_volume)

    # Connect robot control buttons
    global_vars.ui.ButtonZurueck.clicked.connect(lambda: open_page(Page.MAIN_PAGE))
    global_vars.ui.ButtonRoboterStart.clicked.connect(send_cmd_play)
    global_vars.ui.ButtonRoboterPause.clicked.connect(send_cmd_pause)
    global_vars.ui.ButtonRoboterStop.clicked.connect(send_cmd_stop)
    global_vars.ui.ButtonStopRPCServer.clicked.connect(server_stop)
    global_vars.ui.ButtonZurueck_2.clicked.connect(lambda: open_page(Page.MAIN_PAGE))
    global_vars.ui.ButtonDatenSenden_2.clicked.connect(server_thread)
    
    # Connect remote control button
    global_vars.ui.pushButtonSendCommandRemoteControl.clicked.connect(send_remote_control_command)

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
        lambda text: global_vars.settings.settings['display']['specs'].__setitem__('height', int(text) if text else 0))
    global_vars.ui.lineEditDisplayWidth.textChanged.connect(
        lambda text: global_vars.settings.settings['display']['specs'].__setitem__('width', int(text) if text else 0))
    global_vars.ui.lineEditDisplayRefreshRate.textChanged.connect(
        lambda text: global_vars.settings.settings['display']['specs'].__setitem__('refresh_rate', int(text) if text else 0))
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
        lambda text: global_vars.settings.settings['info'].__setitem__('number_of_plans', int(text) if text else 0))
    global_vars.ui.lineEditNumberCycles.textChanged.connect(
        lambda text: global_vars.settings.settings['info'].__setitem__('number_of_use_cycles', int(text) if text else 0))
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
    
    # Connect scanner signal
    from utils.UR20_Server_functions import scanner_signals
    scanner_signals.status_changed.connect(handle_scanner_status)

def setup_password_handling():
    """Set up password handling functionality."""
    def hash_password(password, salt=None):
        if salt is None:
            salt = os.urandom(16)
        salted_password = salt + password.encode()
        hashed_password = hashlib.sha256(salted_password).hexdigest()
        return salt.hex() + '$' + hashed_password

    global_vars.ui.passwordEdit.textChanged.connect(
        lambda text: global_vars.settings.settings['admin'].__setitem__('password', hash_password(text)) if text else None
    )

def setup_components():
    """Initialize and set up the application components."""
    # Initialize components
    set_wordlist()
    global_vars.canvas = initialize_3d_view(global_vars.ui.MatplotLibCanvasFrame)
    load_rob_files()
    
    # Set up settings
    set_settings_line_edits()
    
    # Add palette clear dialog function to UI
    global_vars.ui.show_palette_clear_dialog = show_palette_clear_dialog
    
    # Override the keyPressEvent handler for the EingabePallettenplan input field
    original_key_press = global_vars.ui.EingabePallettenplan.keyPressEvent
    
    def custom_key_press(event):
        # Call the original handler
        original_key_press(event)
        
        # Make sure the field keeps focus after any key press
        # This is essential for keeping the virtual keyboard open
        if event.key() not in (Qt.Key_Escape, Qt.Key_Return, Qt.Key_Enter):
            # Ensure field has focus for keys other than escape/return/enter
            global_vars.ui.EingabePallettenplan.setFocus()
        
    global_vars.ui.EingabePallettenplan.keyPressEvent = custom_key_press

def start_background_tasks():
    """Start background tasks and threads."""
    # Start delay warning sound monitor thread
    warning_sound_thread = threading.Thread(target=delay_warning_sound)
    warning_sound_thread.daemon = True
    warning_sound_thread.start()
    
    # Start zwischenlage popup monitor
    from utils.notification_popup import check_zwischenlage_status
    
    # Create a timer to check zwischenlage status every 500ms
    zwischenlage_timer = QTimer(global_vars.main_window)
    zwischenlage_timer.timeout.connect(check_zwischenlage_status)
    zwischenlage_timer.start(500)  # Check every 500ms
    
    # Create a timer to check palette clearing status every 1000ms
    from utils.ui_helpers import check_palette_clearing_status
    palette_clear_timer = QTimer(global_vars.main_window)
    palette_clear_timer.timeout.connect(check_palette_clearing_status)
    palette_clear_timer.start(1000)  # Check every 1000ms
    
    # Check zwischenlage status immediately (don't wait for timer)
    check_zwischenlage_status()

def setup_window_handling():
    """Set up window close handling and key press events."""
    # Set up window close handling
    global_vars.allow_close = False

    def allow_close_event(event):
        if global_vars.allow_close:
            event.accept()
            global_vars.allow_close = False
        else:
            event.ignore()

    def handle_key_press_event(event):
        logger.debug(f"Key press detected: modifiers={event.modifiers()}, key={event.key()}")
        
        # Check for CTRL+ALT+SHIFT+C
        if (event.modifiers() == (Qt.KeyboardModifier.ControlModifier | 
                                 Qt.KeyboardModifier.AltModifier | 
                                 Qt.KeyboardModifier.ShiftModifier) and 
            event.key() == Qt.Key.Key_C):
            logger.info("Exit key combination detected (CTRL+ALT+SHIFT+C)")
            global_vars.allow_close = True
            exit_app()
        # Alternative implementation for additional reliability
        elif (event.key() == Qt.Key.Key_C and 
              (event.modifiers() & Qt.KeyboardModifier.ControlModifier) and
              (event.modifiers() & Qt.KeyboardModifier.AltModifier) and
              (event.modifiers() & Qt.KeyboardModifier.ShiftModifier)):
            logger.info("Exit key combination detected (alternative check)")
            global_vars.allow_close = True
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
            global_vars.main_window.setWindowState(global_vars.main_window.windowState() ^ Qt.WindowState.WindowActive)

    global_vars.main_window.closeEvent = allow_close_event
    global_vars.main_window.keyPressEvent = handle_key_press_event