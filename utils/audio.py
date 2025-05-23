import logging
import os
import subprocess
import threading
import time
from datetime import datetime
from PySide6.QtGui import QIcon

from utils import global_vars

logger = logging.getLogger(__name__)

# TODO: overhaul everything so that it can be used for other sounds as well and stuff

def spawn_play_stepback_warning_thread():
    """Spawn a thread to play the stepback warning.
    """
    global audio_thread
    logger.info("Starting stepback warning audio thread")
    global_vars.audio_thread_running = True
    audio_thread = threading.Thread(target=play_stepback_warning)
    audio_thread.daemon = True
    audio_thread.start()
    global_vars.audio_thread = audio_thread

def kill_play_stepback_warning_thread():
    """Kill the thread playing the stepback warning.
    """
    logger.info("Stopping stepback warning audio thread")
    global_vars.audio_thread_running = False
    if global_vars.audio_thread:
        global_vars.audio_thread = None

def play_stepback_warning():
    """Play the stepback warning in a loop using aplay.
    """
    logger.debug("Starting stepback warning playback loop")
    
    try:
        while global_vars.audio_thread_running:
            try:
                # Use aplay to play the audio file
                logger.debug(f"Playing audio file: {global_vars.settings.settings['admin']['alarm_sound_file']}")
                if os.name != 'nt':
                    subprocess.run(['aplay', global_vars.settings.settings['admin']['alarm_sound_file']], 
                                    check=True,
                                    stdout=subprocess.DEVNULL,
                                    stderr=subprocess.DEVNULL)
                logger.debug("Stepback warning played successfully")
                time.sleep(0.1)  # Small delay between loops
                
            except subprocess.CalledProcessError as e:
                logger.error(f"Error during audio playback: {e}")
                break
    except Exception as e:
        logger.error(f"Fatal error in audio thread: {e}")
    finally:
        logger.debug("Audio playback thread stopping")

def set_audio_volume() -> None:
    """Set the audio volume.
    """
    if not global_vars.ui:
        logger.error("UI not initialized, cannot set audio volume")
        return
        
    volume = '0' if not global_vars.audio_muted else '100'
    icon_name = ":/Sound/imgs/volume-off.png" if not global_vars.audio_muted else ":/Sound/imgs/volume-on.png"
    
    logger.info(f"Setting audio volume to {volume}")
    try:
        if os.name != 'nt':
            subprocess.run(['amixer', '-c', '3', 'cset', 'name=Max Overclock DAC', volume], 
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL,
                            check=True)
        global_vars.ui.pushButtonVolumeOnOff.setIcon(QIcon(icon_name))
        global_vars.audio_muted = not global_vars.audio_muted
        logger.debug("Audio volume set successfully")
    except Exception as e:
        logger.error(f"Failed to set audio volume: {e}")

def delay_warning_sound():
    """Delay the warning sound start by 40 seconds.
    """
    logger.debug("Starting delay warning sound monitor")
    while True:
        try:
            if global_vars.timestamp_scanner_fault:
                # Handle the case where timestamp_scanner_fault might be a float
                if isinstance(global_vars.timestamp_scanner_fault, float):
                    # If it's a float, assume it's a timestamp and calculate the difference directly
                    current_time = datetime.now().timestamp()
                    delay = current_time - global_vars.timestamp_scanner_fault                    
                if delay >= 40.0:
                    logger.info("40-second delay reached, starting warning sound")
                    if not global_vars.audio_thread_running and not global_vars.audio_muted:
                        spawn_play_stepback_warning_thread()
                    elif global_vars.audio_muted:
                        logger.debug("Warning sound not started - audio is muted")
            if global_vars.timestamp_scanner_fault is None:
                logger.debug("Scanner fault cleared, stopping warning sound")
                kill_play_stepback_warning_thread()
            time.sleep(5)
        except Exception as e:
            logger.error(f"Error in delay warning sound monitor: {e}") 