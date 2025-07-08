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

logger = global_vars.logger
logger.info("audio.py logger initialized")

# Don't initialize pygame mixer at import time - only when needed
_pygame_initialized = False

def _ensure_pygame_initialized():
    """Initialize pygame mixer if not already initialized."""
    global _pygame_initialized
    if not _pygame_initialized:
        try:
            pygame.mixer.init()
            _pygame_initialized = True
            logger.info("Pygame mixer initialized")
        except Exception as e:
            logger.error(f"Failed to initialize pygame mixer: {e}")
            raise

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
        self._start_playback()  # Always try to start playback after adding

    def stop_audio(self, id: str) -> None:
        """Stop playback of a specific audio by ID."""
        with self._lock:
            if self.current_item and self.current_item.id == id:
                self._stop_event.set()
                if _pygame_initialized:
                    pygame.mixer.music.stop()
                self.current_item = None
            self.queue = deque(item for item in self.queue if item.id != id)
            logger.info(f"Stopped audio {id}")

    def kill_all(self) -> None:
        """Stop all audio playback and clear the queue."""
        with self._lock:
            self._stop_event.set()
            if _pygame_initialized:
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
        logger.debug("Starting audio playback loop")
        while not self._stop_event.is_set():
            current_item = None
            with self._lock:
                if not self.queue:
                    logger.debug("Queue is empty, stopping playback loop")
                    break

                self.current_item = self.queue[0]
                current_item = self.current_item
                logger.debug(f"Processing audio item: {current_item.id} (count: {current_item.current_count}/{current_item.playback_count})")
                
                if current_item.playback_count != -1 and current_item.current_count >= current_item.playback_count:
                    logger.debug(f"Removing {current_item.id} from queue - playback count reached")
                    self.queue.popleft()
                    continue

            try:
                # Initialize pygame if not already done
                _ensure_pygame_initialized()
                
                logger.debug(f"Loading audio file: {current_item.file_path}")
                pygame.mixer.music.load(current_item.file_path)
                pygame.mixer.music.play()
                logger.debug(f"Started playing: {current_item.id}")
                
                # Wait for the sound to finish playing
                while pygame.mixer.music.get_busy() and not self._stop_event.is_set():
                    time.sleep(0.1)
                
                logger.debug(f"Finished playing: {current_item.id}")
                
                with self._lock:
                    if current_item.playback_count != -1:
                        current_item.current_count += 1
                        logger.debug(f"Incremented play count for {current_item.id}: {current_item.current_count}")
                        if current_item.current_count >= current_item.playback_count:
                            logger.debug(f"Removing {current_item.id} from queue - reached max plays")
                            self.queue.popleft()
                    # Move the current item to the end of the queue if it's infinite
                    if current_item.playback_count == -1:
                        logger.debug(f"Rotating infinite playback item: {current_item.id}")
                        self.queue.rotate(-1)

            except Exception as e:
                logger.error(f"Error during audio playback: {e}")
                with self._lock:
                    if self.queue and self.queue[0] == current_item:
                        logger.debug(f"Removing failed item from queue: {current_item.id}")
                        self.queue.popleft()
        # Ensure state is reset when playback loop ends
        with self._lock:
            self.is_playing = False
            self._thread = None
        logger.debug("Audio playback loop ended")

# Global audio queue instance
audio_queue = AudioQueue()

def play_audio(audio_id: str, audio_file: str, loop: bool = False) -> None:
    """Helper function to play audio files. Set loop=True for infinite playback."""
    if not global_vars.audio_muted:
        audio_queue.add_to_queue(audio_id, audio_file, -1 if loop else 1)

def add_audio_to_queue(id: str, audio_file: str, playback_count: int) -> None:
    """Add an audio file to the queue with specific playback count."""
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
        if _pygame_initialized:
            pygame.mixer.music.set_volume(volume)
        global_vars.ui.pushButtonVolumeOnOff.setIcon(QIcon(icon_name))
        global_vars.audio_muted = not global_vars.audio_muted
        if global_vars.audio_muted:
            kill_all_audio()
        logger.debug("Audio volume set successfully")
    except Exception as e:
        logger.error(f"Failed to set audio volume: {e}")

def start_safety_monitor_thread():
    """Start the safety status monitor in a daemon thread (call after settings are initialized)."""
    logger.info("Initializing safety monitor thread...")
    thread = threading.Thread(target=monitor_safety_status, daemon=True)
    thread.start()
    logger.info("Safety monitor thread started")
    return thread

def monitor_safety_status():
    """Monitor robot safety status and play warning sound when in REDUCED mode for 30+ seconds."""
    logger.info("Starting safety status monitor thread")
    while not hasattr(global_vars, 'settings') or global_vars.settings is None:
        logger.warning("Settings not available, waiting...")
        time.sleep(1)
    WARNING_SOUND = global_vars.settings.settings['admin']['alarm_sound_file']
    WARNING_SOUND_ID = "safety_warning"
    logger.debug(f"Warning sound path: {WARNING_SOUND}")
    reduced_start_time = None
    while True:
        try:
            if not hasattr(global_vars, 'current_safety_status'):
                logger.warning("Safety status not available, waiting...")
                time.sleep(1)
                continue
            current_status = global_vars.current_safety_status
            logger.debug(f"Current safety status: {current_status}")
            if current_status == SafetyStatus.REDUCED:
                if reduced_start_time is None:
                    reduced_start_time = datetime.now()
                elif (datetime.now() - reduced_start_time).total_seconds() >= 30:
                    # Check if warning sound is not already playing or queued
                    is_playing = (
                        (audio_queue.current_item and audio_queue.current_item.id == WARNING_SOUND_ID)
                        or any(item.id == WARNING_SOUND_ID for item in audio_queue.queue)
                    )
                    logger.debug(f"Warning sound currently playing or queued: {is_playing}")
                    if not is_playing:
                        logger.info("Robot in REDUCED mode for 30+ seconds, starting warning sound")
                        play_audio(WARNING_SOUND_ID, WARNING_SOUND, loop=True)
            else:
                reduced_start_time = None
                # Stop warning sound if robot is not in REDUCED mode
                logger.debug(f"Robot not in REDUCED mode ({current_status}), stopping warning sound if playing")
                stop_audio(WARNING_SOUND_ID)
            time.sleep(1)  # Check every second
        except AttributeError as e:
            logger.error(f"Missing required attribute: {e}")
            time.sleep(1)
        except Exception as e:
            logger.error(f"Error in safety status monitor: {e}")
            time.sleep(1)  # Wait before retrying