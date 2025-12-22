"""Database migration utilities for adding sync support to existing databases."""

import sqlite3
import logging

logger = logging.getLogger(__name__)

def migrate_add_sync_columns(db_path="paletten.db"):
    """Add sync tracking columns to existing SQLite database.
    
    Args:
        db_path: Path to SQLite database file
        
    Returns:
        bool: True if migration was successful
    """
    try:
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        
        # Add sync_status column if it doesn't exist
        try:
            cursor.execute("ALTER TABLE paletten_metadata ADD COLUMN sync_status TEXT")
            logger.info("Added sync_status column to paletten_metadata")
        except sqlite3.OperationalError:
            logger.debug("sync_status column already exists")
        
        # Add sync_timestamp column if it doesn't exist
        try:
            cursor.execute("ALTER TABLE paletten_metadata ADD COLUMN sync_timestamp REAL")
            logger.info("Added sync_timestamp column to paletten_metadata")
        except sqlite3.OperationalError:
            logger.debug("sync_timestamp column already exists")
        
        # Add last_modified column if it doesn't exist
        try:
            cursor.execute("ALTER TABLE paletten_metadata ADD COLUMN last_modified REAL")
            logger.info("Added last_modified column to paletten_metadata")
        except sqlite3.OperationalError:
            logger.debug("last_modified column already exists")
        
        # Set default sync_status for existing records
        cursor.execute('''
            UPDATE paletten_metadata 
            SET sync_status = 'pending', last_modified = file_timestamp
            WHERE sync_status IS NULL
        ''')
        
        conn.commit()
        conn.close()
        
        logger.info("Migration completed successfully")
        return True
    except Exception as e:
        logger.error(f"Migration failed: {e}")
        return False

def export_sqlite_to_postgresql_format(db_path="paletten.db"):
    """Export SQLite data in PostgreSQL-compatible format.
    
    This is a helper function for manual migration.
    
    Args:
        db_path: Path to SQLite database file
        
    Returns:
        list: List of SQL INSERT statements
    """
    # This would generate INSERT statements compatible with PostgreSQL
    # For now, this is a placeholder - actual migration should use
    # the sync functions which handle the conversion automatically
    logger.info("Use sync functions for automatic migration")
    return []

