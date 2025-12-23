"""Hybrid database manager for offline-first database operations.

This module provides a HybridDatabaseManager class that manages both
local SQLite and remote PostgreSQL databases with automatic sync.
"""

import sqlite3
import threading
import time
import logging
from typing import Optional, Tuple, Literal
from enum import Enum

logger = logging.getLogger(__name__)

# Try to import psycopg2, but don't fail if it's not available
try:
    import psycopg2
    from psycopg2 import pool, OperationalError
    PSYCOPG2_AVAILABLE = True
except ImportError:
    PSYCOPG2_AVAILABLE = False
    logger.warning("psycopg2 not available. Remote database features will be disabled.")


class ConnectionStatus(Enum):
    """Connection status enumeration."""
    ONLINE = "online"
    OFFLINE = "offline"
    CONNECTING = "connecting"


class HybridDatabaseManager:
    """Manages both local SQLite and remote PostgreSQL databases with sync.
    
    This class provides:
    - Automatic fallback to SQLite when PostgreSQL is unavailable
    - Background sync thread for pending changes
    - Connection health monitoring
    - Thread-safe operations
    """
    
    def __init__(self, local_db_path: str = "paletten.db",
                 remote_config: Optional[dict] = None):
        """Initialize the hybrid database manager.
        
        Args:
            local_db_path: Path to local SQLite database file
            remote_config: Optional dict with keys: host, port, database, user, password
        """
        self.local_db_path = local_db_path
        self.remote_config = remote_config or {}
        self.connection_status = ConnectionStatus.OFFLINE
        self.remote_pool: Optional[pool.ThreadedConnectionPool] = None
        self.sync_lock = threading.Lock()
        self.last_sync_time: Optional[float] = None
        self.sync_thread: Optional[threading.Thread] = None
        self.sync_running = False
        
        # Initialize local database
        self._init_local_db()
        
        # Try to connect to remote if configured and psycopg2 is available
        if self.remote_config and PSYCOPG2_AVAILABLE:
            if self.remote_config.get('enabled', False):
                self._init_remote_db()
        
        # Start sync thread if remote is configured
        if self.remote_config and self.remote_config.get('enabled', False):
            self._start_sync_thread()
    
    def _init_local_db(self):
        """Initialize local SQLite database."""
        from utils.database.database import create_database
        create_database(self.local_db_path)
        logger.info(f"Local database initialized: {self.local_db_path}")
    
    def _init_remote_db(self):
        """Initialize remote PostgreSQL connection pool."""
        if not PSYCOPG2_AVAILABLE:
            logger.warning("HybridDatabaseManager: psycopg2 not available, cannot initialize remote database")
            return
        try:
            self.connection_status = ConnectionStatus.CONNECTING
            logger.info(
                "HybridDatabaseManager: Connecting to remote DB host=%s port=%s db=%s user=%s",
                self.remote_config.get('host', 'localhost'),
                self.remote_config.get('port', 5432),
                self.remote_config.get('database', 'multipack_parser'),
                self.remote_config.get('user', 'postgres'),
            )
            self.remote_pool = pool.ThreadedConnectionPool(
                1, 10,
                host=self.remote_config.get('host', 'localhost'),
                port=self.remote_config.get('port', 5432),
                database=self.remote_config.get('database', 'multipack_parser'),
                user=self.remote_config.get('user', 'postgres'),
                password=self.remote_config.get('password', ''),
                connect_timeout=5,
            )
            # Test connection
            conn = self.remote_pool.getconn()
            cur = conn.cursor()
            cur.execute("SELECT 1")
            cur.close()
            conn.close()
            self.remote_pool.putconn(conn)
            self.connection_status = ConnectionStatus.ONLINE
            logger.info("HybridDatabaseManager: Remote database connection established")
            # Create remote schema if it doesn't exist
            from utils.database.database import create_remote_database
            create_remote_database(self)
            logger.info("HybridDatabaseManager: Remote database schema created/verified")
        except Exception as e:
            self.connection_status = ConnectionStatus.OFFLINE
            logger.warning(f"HybridDatabaseManager: Remote database unavailable, using local only: {e}")
            self.remote_pool = None
    
    def _check_remote_connection(self) -> bool:
        """Check if remote database is accessible."""
        if not self.remote_pool or not PSYCOPG2_AVAILABLE:
            return False
        try:
            conn = self.remote_pool.getconn()
            cursor = conn.cursor()
            cursor.execute("SELECT 1")
            cursor.close()
            self.remote_pool.putconn(conn)
            if self.connection_status != ConnectionStatus.ONLINE:
                logger.info("HybridDatabaseManager: Remote database connection restored")
            self.connection_status = ConnectionStatus.ONLINE
            return True
        except Exception as e:
            if self.connection_status == ConnectionStatus.ONLINE:
                logger.warning(f"HybridDatabaseManager: Remote database connection lost: {e}")
            self.connection_status = ConnectionStatus.OFFLINE
            return False
    
    def get_connection(self, prefer_remote: bool = True) -> Tuple:
        """Get database connection (remote if available, otherwise local).
        
        Args:
            prefer_remote: If True, try remote first; if False, use local only
            
        Returns:
            Tuple of (connection, connection_type) where connection_type is 'remote' or 'local'
        """
        if prefer_remote and self._check_remote_connection():
            return self.remote_pool.getconn(), 'remote'
        else:
            return sqlite3.connect(self.local_db_path), 'local'
    
    def return_connection(self, conn, conn_type: str):
        """Return connection to pool or close it.
        
        Args:
            conn: The database connection
            conn_type: 'remote' or 'local'
        """
        if conn_type == 'remote' and self.remote_pool:
            try:
                self.remote_pool.putconn(conn)
            except Exception as e:
                logger.error(f"Error returning remote connection: {e}")
        elif conn_type == 'local':
            try:
                conn.close()
            except Exception as e:
                logger.error(f"Error closing local connection: {e}")
    
    def is_online(self) -> bool:
        """Check if remote database is currently online.
        
        Returns:
            True if online, False otherwise
        """
        return self.connection_status == ConnectionStatus.ONLINE
    
    def _start_sync_thread(self):
        """Start background thread for syncing changes."""
        if self.sync_running:
            return
        self.sync_running = True
        self.sync_thread = threading.Thread(target=self._sync_worker, daemon=True)
        self.sync_thread.start()
        logger.info("HybridDatabaseManager: Sync thread started")
    
    def _sync_worker(self):
        """Background worker that periodically syncs local changes to remote."""
        while self.sync_running:
            try:
                time.sleep(30)
                logger.debug("HybridDatabaseManager: Sync tick")
                if self._check_remote_connection():
                    from utils.database.database import sync_local_to_remote
                    logger.info("HybridDatabaseManager: Starting periodic sync to remote")
                    sync_local_to_remote(self)
            except Exception as e:
                logger.error(f"HybridDatabaseManager: Error in sync worker: {e}")
    
    def sync_now(self) -> bool:
        """Attempt immediate sync of pending changes.
        
        Returns:
            True if sync was successful, False otherwise
        """
        if not self.is_online():
            return False
        
        with self.sync_lock:
            from utils.database.database import sync_local_to_remote
            return sync_local_to_remote(self)
    
    def force_sync(self) -> bool:
        """Force a full sync (same as sync_now for now).
        
        Returns:
            True if sync was successful, False otherwise
        """
        return self.sync_now()
    
    def close(self):
        """Close all connections and stop sync thread."""
        self.sync_running = False
        if self.sync_thread:
            self.sync_thread.join(timeout=5)
        if self.remote_pool:
            try:
                self.remote_pool.closeall()
            except Exception as e:
                logger.error(f"Error closing remote pool: {e}")

