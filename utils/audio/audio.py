import logging
import os
import threading
import time
from datetime import datetime
from PySide6.QtGui import QIcon
from collections import deque
from dataclasses import dataclass
from typing import Dict, Optional
import pygame

from utils.system.core import global_vars
from utils.robot.robot_enums import SafetyStatus

logger = logging.getLogger(__name__)

# Initialize pygame mixer
pygame.mixer.init()

@dataclass
class AudioItem:
    id: str
    file_path: str
    playback_count: int  # -1 for infinite, positive number for specific count
    current_count: int = 0

class AudioQueue:
    def __init__(self):
        self.queue: deque[AudioItem] = deque()
        self.current_item: Optional[AudioItem] = None
        self.is_playing: bool = False
        self._lock = threading.Lock()
        self._stop_event = threading.Event()
        self._thread: Optional[threading.Thread] = None

    def add_to_queue(self, id: str, audio_file: str, playback_count: int) -> None:
        """Add an audio file to the queue with specified playback count."""
        with self._lock:
            self.queue.append(AudioItem(id, audio_file, playback_count))
            logger.info(f"Added audio {id} to queue with {playback_count} plays")
            if not self.is_playing:
                self._start_playback()

    def stop_audio(self, id: str) -> None:
        """Stop playback of a specific audio by ID."""
        with self._lock:
            if self.current_item and self.current_item.id == id:
                self._stop_event.set()
                pygame.mixer.music.stop()
                self.current_item = None
            self.queue = deque(item for item in self.queue if item.id != id)
            logger.info(f"Stopped audio {id}")

    def kill_all(self) -> None:
        """Stop all audio playback and clear the queue."""
        with self._lock:
            self._stop_event.set()
            pygame.mixer.music.stop()
            self.queue.clear()
            self.current_item = None
            self.is_playing = False
            logger.info("Killed all audio playback")

    def _start_playback(self) -> None:
        """Start the playback thread if it's not already running."""
        if not self.is_playing:
            self.is_playing = True
            self._stop_event.clear()
            self._thread = threading.Thread(target=self._playback_loop)
            self._thread.daemon = True
            self._thread.start()

    def _playback_loop(self) -> None:
        """Main playback loop that handles the queue."""
        while not self._stop_event.is_set():
            current_item = None
            with self._lock:
                if not self.queue:
                    self.is_playing = False
                    break

                self.current_item = self.queue[0]
                current_item = self.current_item
                if current_item.playback_count != -1 and current_item.current_count >= current_item.playback_count:
                    self.queue.popleft()
                    continue

            try:
                pygame.mixer.music.load(current_item.file_path)
                pygame.mixer.music.play()
                
                # Wait for the sound to finish playing
                while pygame.mixer.music.get_busy() and not self._stop_event.is_set():
                    time.sleep(0.1)
                
                with self._lock:
                    if current_item.playback_count != -1:
                        current_item.current_count += 1
                        if current_item.current_count >= current_item.playback_count:
                            self.queue.popleft()
                    # Move the current item to the end of the queue if it's infinite
                    if current_item.playback_count == -1:
                        self.queue.rotate(-1)

            except Exception as e:
                logger.error(f"Error during audio playback: {e}")
                with self._lock:
                    if self.queue and self.queue[0] == current_item:
                        self.queue.popleft()

# Global audio queue instance
audio_queue = AudioQueue()

def add_audio_to_queue(id: str, audio_file: str, playback_count: int) -> None:
    """Add an audio file to the queue."""
    if not global_vars.audio_muted:
        audio_queue.add_to_queue(id, audio_file, playback_count)

def stop_audio(id: str) -> None:
    """Stop a specific audio by ID."""
    audio_queue.stop_audio(id)

def kill_all_audio() -> None:
    """Stop all audio playback."""
    audio_queue.kill_all()

def set_audio_volume() -> None:
    """Set the audio volume."""
    if not global_vars.ui:
        logger.error("UI not initialized, cannot set audio volume")
        return
        
    volume = 0.0 if not global_vars.audio_muted else 1.0
    icon_name = ":/Sound/imgs/volume-off.png" if not global_vars.audio_muted else ":/Sound/imgs/volume-on.png"
    
    logger.info(f"Setting audio volume to {volume}")
    try:
        pygame.mixer.music.set_volume(volume)
        global_vars.ui.pushButtonVolumeOnOff.setIcon(QIcon(icon_name))
        global_vars.audio_muted = not global_vars.audio_muted
        if global_vars.audio_muted:
            kill_all_audio()
        logger.debug("Audio volume set successfully")
    except Exception as e:
        logger.error(f"Failed to set audio volume: {e}")

def monitor_safety_status():
    """Monitor robot safety status and play warning sound when in REDUCED mode."""
    WARNING_SOUND = global_vars.settings.settings['admin']['alarm_sound_file']
    WARNING_SOUND_ID = "safety_warning"
    
    while True:
        try:
            current_status = global_vars.current_safety_status
            logger.debug(current_status)
            if current_status == SafetyStatus.REDUCED:
                # Check if warning sound is not already playing
                if not any(item.id == WARNING_SOUND_ID for item in audio_queue.queue):
                    logger.info("Robot in REDUCED mode, starting warning sound")
                    add_audio_to_queue(WARNING_SOUND_ID, WARNING_SOUND, -1)
            else:
                # Stop warning sound if robot is not in REDUCED mode
                stop_audio(WARNING_SOUND_ID)
                
            time.sleep(1)  # Check every second
            
        except Exception as e:
            logger.error(f"Error in safety status monitor: {e}")
            time.sleep(1)  # Wait before retrying

# Start the safety status monitor in a daemon thread
safety_monitor_thread = threading.Thread(target=monitor_safety_status, daemon=True)
safety_monitor_thread.start() 