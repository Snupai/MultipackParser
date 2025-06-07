import logging
import os
import subprocess
import threading
import time
from datetime import datetime
from PySide6.QtGui import QIcon
from collections import deque
from dataclasses import dataclass
from typing import Dict, Optional

from utils.system.core import global_vars

logger = logging.getLogger(__name__)

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
                self.current_item = None
            self.queue = deque(item for item in self.queue if item.id != id)
            logger.info(f"Stopped audio {id}")

    def kill_all(self) -> None:
        """Stop all audio playback and clear the queue."""
        with self._lock:
            self._stop_event.set()
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
            with self._lock:
                if not self.queue:
                    self.is_playing = False
                    break

                self.current_item = self.queue[0]
                if self.current_item.playback_count != -1 and self.current_item.current_count >= self.current_item.playback_count:
                    self.queue.popleft()
                    continue

            try:
                if os.name != 'nt':
                    subprocess.run(['aplay', self.current_item.file_path],
                                 check=True,
                                 stdout=subprocess.DEVNULL,
                                 stderr=subprocess.DEVNULL)
                
                with self._lock:
                    if self.current_item.playback_count != -1:
                        self.current_item.current_count += 1
                        if self.current_item.current_count >= self.current_item.playback_count:
                            self.queue.popleft()
                    # Move the current item to the end of the queue if it's infinite
                    if self.current_item.playback_count == -1:
                        self.queue.rotate(-1)

            except subprocess.CalledProcessError as e:
                logger.error(f"Error during audio playback: {e}")
                with self._lock:
                    self.queue.popleft()
            except Exception as e:
                logger.error(f"Unexpected error in playback loop: {e}")
                with self._lock:
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
        if global_vars.audio_muted:
            kill_all_audio()
        logger.debug("Audio volume set successfully")
    except Exception as e:
        logger.error(f"Failed to set audio volume: {e}") 