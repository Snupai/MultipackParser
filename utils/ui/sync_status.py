"""Sync status manager for UI display."""

from PySide6.QtCore import QTimer, Signal, QObject
import logging

logger = logging.getLogger(__name__)

class SyncStatusManager(QObject):
    """Manages sync status display in UI."""
    
    status_changed = Signal(str, bool)  # status_text, is_online
    
    def __init__(self, db_manager):
        """Initialize sync status manager.
        
        Args:
            db_manager: HybridDatabaseManager instance
        """
        super().__init__()
        self.db_manager = db_manager
        self.timer = QTimer()
        self.timer.timeout.connect(self._check_status)
        self.timer.start(5000)  # Check every 5 seconds
        self._check_status()
    
    def _check_status(self):
        """Check and update sync status."""
        if not self.db_manager:
            self.status_changed.emit("Database Manager Not Available", False)
            return
        
        is_online = self.db_manager.is_online()
        
        if is_online:
            status = "Online - Synced"
            if self.db_manager.last_sync_time:
                status += f" ({self._format_time(self.db_manager.last_sync_time)})"
            
            # Check for pending syncs
            from utils.database.database import get_sync_status
            sync_info = get_sync_status(self.db_manager)
            if sync_info.get('pending_count', 0) > 0:
                status += f" - {sync_info['pending_count']} pending"
        else:
            status = "Offline - Using Local Database"
        
        self.status_changed.emit(status, is_online)
    
    def _format_time(self, timestamp: float) -> str:
        """Format timestamp for display.
        
        Args:
            timestamp: Unix timestamp
            
        Returns:
            Formatted time string
        """
        from datetime import datetime
        dt = datetime.fromtimestamp(timestamp)
        return dt.strftime("%H:%M:%S")
    
    def stop(self):
        """Stop the status monitoring timer."""
        self.timer.stop()

