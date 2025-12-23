import sqlite3
import os
import time
import datetime
import logging
from typing import Union, List, Dict, Any, Optional, Tuple, Literal

from utils.system.core import global_vars

logger = logging.getLogger(__name__)

def create_database(db_path="paletten.db"):
    """Create the database and tables if they don't exist."""
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    # Create tables for all the global data structures
    cursor.execute('''
    CREATE TABLE IF NOT EXISTS paletten_metadata (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        paket_quer INTEGER,
        center_of_gravity_x REAL,
        center_of_gravity_y REAL,
        center_of_gravity_z REAL,
        lage_arten INTEGER,
        anz_lagen INTEGER,
        anzahl_pakete INTEGER,
        file_timestamp REAL,
        file_name TEXT
    )
    ''')
    
    # Create an index on file_name for faster searches
    cursor.execute('''
    CREATE INDEX IF NOT EXISTS idx_file_name ON paletten_metadata(file_name)
    ''')
    
    cursor.execute('''
    CREATE TABLE IF NOT EXISTS daten (
        id INTEGER PRIMARY KEY,
        metadata_id INTEGER,
        row_index INTEGER,
        col_index INTEGER,
        value INTEGER,
        FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
    )
    ''')
    
    cursor.execute('''
    CREATE TABLE IF NOT EXISTS paletten_dim (
        id INTEGER PRIMARY KEY,
        metadata_id INTEGER,
        length INTEGER,
        width INTEGER, 
        height INTEGER,
        FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
    )
    ''')
    
    cursor.execute('''
    CREATE TABLE IF NOT EXISTS paket_dim (
        id INTEGER PRIMARY KEY,
        metadata_id INTEGER,
        length INTEGER,
        width INTEGER, 
        height INTEGER,
        gap INTEGER,
        weight REAL,
        einzelpaket_laengs INTEGER,
        FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
    )
    ''')
    
    # Add weight column if it doesn't exist (migration for existing databases)
    try:
        cursor.execute("ALTER TABLE paket_dim ADD COLUMN weight REAL")
    except sqlite3.OperationalError:
        pass  # Column already exists
    
    # Add einzelpaket_laengs column if it doesn't exist (migration for existing databases)
    try:
        cursor.execute("ALTER TABLE paket_dim ADD COLUMN einzelpaket_laengs INTEGER")
    except sqlite3.OperationalError:
        pass  # Column already exists
    
    # Add sync tracking columns if they don't exist (migration for existing databases)
    try:
        cursor.execute("ALTER TABLE paletten_metadata ADD COLUMN sync_status TEXT")
    except sqlite3.OperationalError:
        pass  # Column already exists
    
    try:
        cursor.execute("ALTER TABLE paletten_metadata ADD COLUMN sync_timestamp REAL")
    except sqlite3.OperationalError:
        pass  # Column already exists
    
    try:
        cursor.execute("ALTER TABLE paletten_metadata ADD COLUMN last_modified REAL")
    except sqlite3.OperationalError:
        pass  # Column already exists
    
    cursor.execute('''
    CREATE TABLE IF NOT EXISTS lage_zuordnung (
        id INTEGER PRIMARY KEY,
        metadata_id INTEGER,
        lage_index INTEGER,
        value INTEGER,
        FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
    )
    ''')
    
    cursor.execute('''
    CREATE TABLE IF NOT EXISTS zwischenlagen (
        id INTEGER PRIMARY KEY,
        metadata_id INTEGER,
        lage_index INTEGER,
        value INTEGER,
        FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
    )
    ''')
    
    cursor.execute('''
    CREATE TABLE IF NOT EXISTS pakete_zuordnung (
        id INTEGER PRIMARY KEY,
        metadata_id INTEGER,
        lage_index INTEGER,
        value INTEGER,
        FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
    )
    ''')
    
    cursor.execute('''
    CREATE TABLE IF NOT EXISTS paket_pos (
        id INTEGER PRIMARY KEY,
        metadata_id INTEGER,
        paket_index INTEGER,
        xp INTEGER,
        yp INTEGER,
        ap INTEGER,
        xd INTEGER,
        yd INTEGER,
        ad INTEGER,
        nop INTEGER,
        xvec INTEGER,
        yvec INTEGER,
        FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
    )
    ''')
    
    # Enable foreign key support
    cursor.execute("PRAGMA foreign_keys = ON")
    
    conn.commit()
    conn.close()

def create_remote_database(db_manager):
    """Create PostgreSQL database schema if it doesn't exist.
    
    Args:
        db_manager: HybridDatabaseManager instance
    """
    if not db_manager or not db_manager.is_online():
        return
    
    try:
        conn = db_manager.remote_pool.getconn()
        cursor = conn.cursor()
        
        # Create paletten_metadata table with PostgreSQL syntax
        cursor.execute('''
        CREATE TABLE IF NOT EXISTS paletten_metadata (
            id SERIAL PRIMARY KEY,
            paket_quer INTEGER,
            center_of_gravity_x DOUBLE PRECISION,
            center_of_gravity_y DOUBLE PRECISION,
            center_of_gravity_z DOUBLE PRECISION,
            lage_arten INTEGER,
            anz_lagen INTEGER,
            anzahl_pakete INTEGER,
            file_timestamp DOUBLE PRECISION,
            file_name TEXT,
            sync_status VARCHAR(20) DEFAULT 'synced',
            sync_timestamp DOUBLE PRECISION,
            last_modified DOUBLE PRECISION DEFAULT EXTRACT(EPOCH FROM NOW()),
            UNIQUE(file_name)
        )
        ''')
        
        # Create index on file_name
        cursor.execute('''
        CREATE INDEX IF NOT EXISTS idx_file_name ON paletten_metadata(file_name)
        ''')
        
        # Create other tables with PostgreSQL syntax
        cursor.execute('''
        CREATE TABLE IF NOT EXISTS daten (
            id SERIAL PRIMARY KEY,
            metadata_id INTEGER,
            row_index INTEGER,
            col_index INTEGER,
            value INTEGER,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
        ''')
        
        cursor.execute('''
        CREATE TABLE IF NOT EXISTS paletten_dim (
            id SERIAL PRIMARY KEY,
            metadata_id INTEGER,
            length INTEGER,
            width INTEGER, 
            height INTEGER,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
        ''')
        
        cursor.execute('''
        CREATE TABLE IF NOT EXISTS paket_dim (
            id SERIAL PRIMARY KEY,
            metadata_id INTEGER,
            length INTEGER,
            width INTEGER, 
            height INTEGER,
            gap INTEGER,
            weight DOUBLE PRECISION,
            einzelpaket_laengs INTEGER,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
        ''')
        
        cursor.execute('''
        CREATE TABLE IF NOT EXISTS lage_zuordnung (
            id SERIAL PRIMARY KEY,
            metadata_id INTEGER,
            lage_index INTEGER,
            value INTEGER,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
        ''')
        
        cursor.execute('''
        CREATE TABLE IF NOT EXISTS zwischenlagen (
            id SERIAL PRIMARY KEY,
            metadata_id INTEGER,
            lage_index INTEGER,
            value INTEGER,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
        ''')
        
        cursor.execute('''
        CREATE TABLE IF NOT EXISTS pakete_zuordnung (
            id SERIAL PRIMARY KEY,
            metadata_id INTEGER,
            lage_index INTEGER,
            value INTEGER,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
        ''')
        
        cursor.execute('''
        CREATE TABLE IF NOT EXISTS paket_pos (
            id SERIAL PRIMARY KEY,
            metadata_id INTEGER,
            paket_index INTEGER,
            xp INTEGER,
            yp INTEGER,
            ap INTEGER,
            xd INTEGER,
            yd INTEGER,
            ad INTEGER,
            nop INTEGER,
            xvec INTEGER,
            yvec INTEGER,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
        ''')
        
        conn.commit()
        db_manager.return_connection(conn, 'remote')
        logger.info("Remote database schema created/verified")
    except Exception as e:
        logger.error(f"Error creating remote database schema: {e}")
        if conn:
            db_manager.return_connection(conn, 'remote')

def UR_ReadDataFromUsbStick(filename: str, path_usb_stick: str) -> Union[Literal[0], Literal[1]]:
    """Read data from a .rob file on the USB stick and parse it into global variables.
    
    The .rob file contains data about pallet and package dimensions, layer types and positions.
    File format is tab-separated values with the following structure:
    - Line 1: Pallet dimensions (length, width, height)
    - Line 2: Package dimensions (length, width, height, gap)
    - Line 3: Number of layer types
    - Line 4: Number of layers
    - Lines 5+: Layer assignments and intermediate layers
    - Remaining lines: Package positions for each layer type
    
    Args:
        filename (str): Name of the .rob file to read
        path_usb_stick (str): Path to the USB stick directory
        
    Returns:
        Union[Literal[0], Literal[1]]: 0 if successful, 1 if error
        str: File path if successful, None if error
        float: File timestamp if successful, None if error
    """
    # Initialize all global variables used to store the parsed data
    
    # Reset all globals to initial state
    g_Daten = []
    g_LageZuordnung = []
    g_PaketPos = []
    g_PaketeZuordnung = []
    g_Zwischenlagen = []
    g_paket_quer = 1
    g_CenterOfGravity = [0,0,0]
        
    file_path = path_usb_stick + filename
    try:
        # Get file modification time for database tracking
        file_timestamp = os.path.getmtime(file_path)
        
        # Try different encodings in order of likelihood
        encodings = ['utf-8', 'latin1', 'cp1252', 'ascii']
        lines = None
        
        for encoding in encodings:
            try:
                with open(file_path, 'r', encoding=encoding) as f:
                    # Read and parse the file line by line
                    for line in f:
                        # Convert each line to list of integers
                        tmpList = [int(x) for x in line.strip().split('\t')]
                        g_Daten.append(tmpList)
                    # If we get here, the encoding worked
                    break
            except UnicodeDecodeError:
                continue
            except ValueError as e:
                logger.error(f"Error parsing values in file {filename}: {e}")
                return None, None, None, None, None, None, None, None, None, None, None, None, None, None
        
        if not g_Daten:
            logger.error(f"Could not decode file {filename} with any of the attempted encodings")
            return None, None, None, None, None, None, None, None, None, None, None, None, None, None

        # Parse pallet dimensions from first line
        pl = g_Daten[global_vars.LI_PALETTE_DATA][global_vars.LI_PALETTE_DATA_LENGTH]  # Length
        pw = g_Daten[global_vars.LI_PALETTE_DATA][global_vars.LI_PALETTE_DATA_WIDTH]   # Width
        ph = g_Daten[global_vars.LI_PALETTE_DATA][global_vars.LI_PALETTE_DATA_HEIGHT]  # Height
        g_PalettenDim = [pl, pw, ph]
        
        # Parse package dimensions from second line
        pl = g_Daten[global_vars.LI_PACKAGE_DATA][global_vars.LI_PACKAGE_DATA_LENGTH]  # Length
        pw = g_Daten[global_vars.LI_PACKAGE_DATA][global_vars.LI_PACKAGE_DATA_WIDTH]   # Width
        ph = g_Daten[global_vars.LI_PACKAGE_DATA][global_vars.LI_PACKAGE_DATA_HEIGHT]  # Height
        pr = g_Daten[global_vars.LI_PACKAGE_DATA][global_vars.LI_PACKAGE_DATA_GAP]     # Gap between packages
        g_PaketDim = [pl, pw, ph, pr]

        # Get number of layer types from third line
        g_LageArten = g_Daten[global_vars.LI_LAYERTYPES][0]
        
        # Get number of layers from fourth line
        anzLagen = g_Daten[global_vars.LI_NUMBER_OF_LAYERS][0]
        g_AnzLagen = anzLagen

        # Parse layer assignments and intermediate layers
        index = global_vars.LI_NUMBER_OF_LAYERS + 2  # Skip header lines
        end_index = index + anzLagen

        while index < end_index:
            lagenart = g_Daten[index][0]  # Layer type
            zwischenlagen = g_Daten[index][1]  # Intermediate layer flag

            g_LageZuordnung.append(lagenart)
            g_Zwischenlagen.append(zwischenlagen)
            index = index + 1
        
        # Parse package positions
        ersteLage = 4 + (anzLagen + 1)  # Skip layer data
        index = ersteLage
        anzahlPaket = g_Daten[index][0]  # Total packages
        g_AnzahlPakete = anzahlPaket  # Note: Deprecated - now number of picks for multipick
        index_paketZuordnung = index
        
        # Get number of packages per layer type
        for i in range(g_LageArten):
            anzahlPick = g_Daten[index_paketZuordnung][0]
            g_PaketeZuordnung.append(anzahlPick)
            index_paketZuordnung = index_paketZuordnung + anzahlPick + 1
        
        # Parse individual package positions for each layer type
        for i in range(g_LageArten):            
            index = index + 1  # Skip count line
            anzahlPaket = g_PaketeZuordnung[i]
            
            for j in range(anzahlPaket):
                # Extract position data:
                xp = g_Daten[index][global_vars.LI_POSITION_XP]    # X pick position
                yp = g_Daten[index][global_vars.LI_POSITION_YP]    # Y pick position
                ap = g_Daten[index][global_vars.LI_POSITION_AP]    # Pick angle
                xd = g_Daten[index][global_vars.LI_POSITION_XD]    # X drop position
                yd = g_Daten[index][global_vars.LI_POSITION_YD]    # Y drop position
                ad = g_Daten[index][global_vars.LI_POSITION_AD]    # Drop angle
                nop = g_Daten[index][global_vars.LI_POSITION_NOP]  # Number of packages
                xvec = g_Daten[index][global_vars.LI_POSITION_XVEC]  # X vector
                yvec = g_Daten[index][global_vars.LI_POSITION_YVEC]  # Y vector
                packagePos = [xp, yp, ap, xd, yd, ad, nop, xvec, yvec]
                g_PaketPos.append(packagePos)
                index = index + 1
        
        return file_path, file_timestamp, g_Daten, g_LageZuordnung, g_PaketPos, g_PaketeZuordnung, g_Zwischenlagen, g_paket_quer, g_CenterOfGravity, g_PalettenDim, g_PaketDim, g_LageArten, g_AnzLagen, g_AnzahlPakete
            
    except Exception as e:
        logger.error(f"Error reading file {filename}: {e}")
        return None, None, None, None, None, None, None, None, None, None, None, None, None, None

def save_to_database(file_name, db_path="paletten.db", db_manager=None) -> bool:
    """Save all global data to the database.
    
    Args:
        file_name (str): Name of the file to save
        db_path (str): Path to the database (deprecated if db_manager provided)
        db_manager: Optional HybridDatabaseManager instance for hybrid sync
        
    Returns:
        bool: True if data was saved, False if skipped due to older timestamp
    """
    # Use db_manager if provided, otherwise use legacy SQLite-only mode
    if db_manager:
        return _save_with_sync(file_name, db_manager)
    else:
        return _save_sqlite_only(file_name, db_path)

def _save_sqlite_only(file_name, db_path="paletten.db") -> bool:
    """Legacy SQLite-only save function."""
    # Create database tables if they don't exist
    create_database(db_path)
    logger.info(f"Handling file: {file_name}")
    _, file_timestamp, g_Daten, g_LageZuordnung, g_PaketPos, g_PaketeZuordnung, g_Zwischenlagen, g_paket_quer, g_CenterOfGravity, g_PalettenDim, g_PaketDim, g_LageArten, g_AnzLagen, g_AnzahlPakete = UR_ReadDataFromUsbStick(file_name, global_vars.PATH_USB_STICK)
    
    # If parsing failed, skip updating the database
    if (
        file_timestamp is None or
        g_Daten is None or len(g_Daten) == 0 or
        g_LageZuordnung is None or g_PaketPos is None or
        g_PaketeZuordnung is None or g_Zwischenlagen is None or
        g_PalettenDim is None or g_PaketDim is None
    ):
        logger.error(f"Skipping database update for '{file_name}' due to parse failure or missing data")
        return False
    
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    # Enable foreign key support for data integrity
    cursor.execute("PRAGMA foreign_keys = ON")
    
    # Check if this file already exists in database by matching file name
    existing_metadata_id = None
    if file_name:
        cursor.execute('''
        SELECT id, file_timestamp FROM paletten_metadata 
        WHERE file_name = ?
        ''', (file_name,))
        result = cursor.fetchone()
        
        if result:
            existing_metadata_id, existing_timestamp = result
            
            # Skip update if existing data is newer or same age
            if file_timestamp is not None and existing_timestamp >= file_timestamp:
                logger.info(f"Skipping database update - existing data is newer or same age.")
                logger.info(f"Existing: {datetime.datetime.fromtimestamp(existing_timestamp)}")
                logger.info(f"New file: {datetime.datetime.fromtimestamp(file_timestamp)}")
                conn.close()
                return False
    
    # Delete existing data if we're updating a file
    # CASCADE will automatically delete related data in other tables
    if existing_metadata_id:
        logger.info(f"Updating existing file data (ID: {existing_metadata_id})")
        cursor.execute("DELETE FROM paletten_metadata WHERE id = ?", (existing_metadata_id,))
    
    
    
    # Insert main metadata record with core parameters
    cursor.execute('''
    INSERT INTO paletten_metadata (
        paket_quer, center_of_gravity_x, center_of_gravity_y, center_of_gravity_z, 
        lage_arten, anz_lagen, anzahl_pakete, file_timestamp, file_name
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    ''', (g_paket_quer, g_CenterOfGravity[0], g_CenterOfGravity[1], 
          g_CenterOfGravity[2], g_LageArten, g_AnzLagen, 
          g_AnzahlPakete, file_timestamp, file_name))
    
    # Get ID of new metadata record for linking related data
    metadata_id = cursor.lastrowid
    
    # Save raw data array from .rob file
    for i, row in enumerate(g_Daten):
        for j, value in enumerate(row):
            cursor.execute('''
            INSERT INTO daten (metadata_id, row_index, col_index, value) 
            VALUES (?, ?, ?, ?)
            ''', (metadata_id, i, j, value))
    
    # Save pallet dimensions if available
    if g_PalettenDim:
        cursor.execute('''
        INSERT INTO paletten_dim (metadata_id, length, width, height) 
        VALUES (?, ?, ?, ?)
        ''', (metadata_id, g_PalettenDim[0], g_PalettenDim[1], 
              g_PalettenDim[2]))
    
    # Save package dimensions if available
    if g_PaketDim:
        cursor.execute('''
        INSERT INTO paket_dim (metadata_id, length, width, height, gap, weight) 
        VALUES (?, ?, ?, ?, ?, ?)
        ''', (metadata_id, g_PaketDim[0], g_PaketDim[1], 
              g_PaketDim[2], g_PaketDim[3], None))
    
    # Save layer type assignments (which type is each layer)
    for i, value in enumerate(g_LageZuordnung):
        cursor.execute('''
        INSERT INTO lage_zuordnung (metadata_id, lage_index, value) 
        VALUES (?, ?, ?)
        ''', (metadata_id, i, value))
    
    # Save intermediate layer flags (whether each layer has separator)
    for i, value in enumerate(g_Zwischenlagen):
        cursor.execute('''
        INSERT INTO zwischenlagen (metadata_id, lage_index, value) 
        VALUES (?, ?, ?)
        ''', (metadata_id, i, value))
    
    # Save number of packages per layer type
    for i, value in enumerate(g_PaketeZuordnung):
        cursor.execute('''
        INSERT INTO pakete_zuordnung (metadata_id, lage_index, value) 
        VALUES (?, ?, ?)
        ''', (metadata_id, i, value))
    
    # Save package positions with pick/place coordinates and angles
    for i, pos in enumerate(g_PaketPos):
        cursor.execute('''
        INSERT INTO paket_pos (metadata_id, paket_index, xp, yp, ap, xd, yd, ad, nop, xvec, yvec) 
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ''', (metadata_id, i, pos[0], pos[1], pos[2], pos[3], pos[4], pos[5], pos[6], pos[7], pos[8]))
    
    logger.info(f"Saved file: {file_name} to database")
    conn.commit()
    conn.close()
    return True

def _save_with_sync(file_name: str, db_manager) -> bool:
    """Save to database with sync support (offline-first approach).
    
    Args:
        file_name: Name of the file to save
        db_manager: HybridDatabaseManager instance
        
    Returns:
        bool: True if saved successfully
    """
    logger.info(f"Handling file: {file_name}")
    _, file_timestamp, g_Daten, g_LageZuordnung, g_PaketPos, g_PaketeZuordnung, g_Zwischenlagen, g_paket_quer, g_CenterOfGravity, g_PalettenDim, g_PaketDim, g_LageArten, g_AnzLagen, g_AnzahlPakete = UR_ReadDataFromUsbStick(file_name, global_vars.PATH_USB_STICK)
    
    # If parsing failed, skip updating the database
    if (
        file_timestamp is None or
        g_Daten is None or len(g_Daten) == 0 or
        g_LageZuordnung is None or g_PaketPos is None or
        g_PaketeZuordnung is None or g_Zwischenlagen is None or
        g_PalettenDim is None or g_PaketDim is None
    ):
        logger.error(f"Skipping database update for '{file_name}' due to parse failure or missing data")
        return False
    
    # Always save to local first (offline-first)
    local_conn = sqlite3.connect(db_manager.local_db_path)
    local_cursor = local_conn.cursor()
    local_cursor.execute("PRAGMA foreign_keys = ON")
    
    # Check if exists
    local_cursor.execute('''
        SELECT id, file_timestamp, sync_status FROM paletten_metadata 
        WHERE file_name = ?
    ''', (file_name,))
    existing = local_cursor.fetchone()
    
    sync_status = 'pending'  # New or modified, needs sync
    if existing:
        existing_id, existing_ts, existing_sync = existing
        if existing_ts and file_timestamp and existing_ts >= file_timestamp:
            local_conn.close()
            return False  # No update needed
        sync_status = 'modified'
        # Delete existing data
        local_cursor.execute("DELETE FROM paletten_metadata WHERE id = ?", (existing_id,))
    
    current_time = time.time()
    
    # Insert main metadata record
    local_cursor.execute('''
    INSERT INTO paletten_metadata (
        paket_quer, center_of_gravity_x, center_of_gravity_y, center_of_gravity_z, 
        lage_arten, anz_lagen, anzahl_pakete, file_timestamp, file_name,
        sync_status, sync_timestamp, last_modified
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    ''', (g_paket_quer, g_CenterOfGravity[0], g_CenterOfGravity[1], 
          g_CenterOfGravity[2], g_LageArten, g_AnzLagen, 
          g_AnzahlPakete, file_timestamp, file_name,
          sync_status, None, current_time))
    
    metadata_id = local_cursor.lastrowid
    
    # Save all related data (same as _save_sqlite_only)
    for i, row in enumerate(g_Daten):
        for j, value in enumerate(row):
            local_cursor.execute('''
            INSERT INTO daten (metadata_id, row_index, col_index, value) 
            VALUES (?, ?, ?, ?)
            ''', (metadata_id, i, j, value))
    
    if g_PalettenDim:
        local_cursor.execute('''
        INSERT INTO paletten_dim (metadata_id, length, width, height) 
        VALUES (?, ?, ?, ?)
        ''', (metadata_id, g_PalettenDim[0], g_PalettenDim[1], g_PalettenDim[2]))
    
    if g_PaketDim:
        local_cursor.execute('''
        INSERT INTO paket_dim (metadata_id, length, width, height, gap, weight) 
        VALUES (?, ?, ?, ?, ?, ?)
        ''', (metadata_id, g_PaketDim[0], g_PaketDim[1], 
              g_PaketDim[2], g_PaketDim[3], None))
    
    for i, value in enumerate(g_LageZuordnung):
        local_cursor.execute('''
        INSERT INTO lage_zuordnung (metadata_id, lage_index, value) 
        VALUES (?, ?, ?)
        ''', (metadata_id, i, value))
    
    for i, value in enumerate(g_Zwischenlagen):
        local_cursor.execute('''
        INSERT INTO zwischenlagen (metadata_id, lage_index, value) 
        VALUES (?, ?, ?)
        ''', (metadata_id, i, value))
    
    for i, value in enumerate(g_PaketeZuordnung):
        local_cursor.execute('''
        INSERT INTO pakete_zuordnung (metadata_id, lage_index, value) 
        VALUES (?, ?, ?)
        ''', (metadata_id, i, value))
    
    for i, pos in enumerate(g_PaketPos):
        local_cursor.execute('''
        INSERT INTO paket_pos (metadata_id, paket_index, xp, yp, ap, xd, yd, ad, nop, xvec, yvec) 
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ''', (metadata_id, i, pos[0], pos[1], pos[2], pos[3], pos[4], pos[5], pos[6], pos[7], pos[8]))
    
    local_conn.commit()
    local_conn.close()
    
    # Try to save to remote immediately if online
    if db_manager.is_online():
        try:
            _save_to_remote(db_manager, file_name, file_timestamp, 
                          g_Daten, g_LageZuordnung, g_PaketPos, 
                          g_PaketeZuordnung, g_Zwischenlagen, 
                          g_paket_quer, g_CenterOfGravity, 
                          g_PalettenDim, g_PaketDim, 
                          g_LageArten, g_AnzLagen, g_AnzahlPakete)
            
            # Mark as synced
            local_conn = sqlite3.connect(db_manager.local_db_path)
            local_cursor = local_conn.cursor()
            local_cursor.execute('''
                UPDATE paletten_metadata 
                SET sync_status = 'synced', sync_timestamp = ?
                WHERE file_name = ?
            ''', (time.time(), file_name))
            local_conn.commit()
            local_conn.close()
            logger.info(f"Saved and synced file: {file_name}")
        except Exception as e:
            logger.warning(f"Failed to save to remote, will sync later: {e}")
            logger.info(f"Saved file: {file_name} to local database (pending sync)")
    else:
        logger.info(f"Saved file: {file_name} to local database (offline)")
    
    return True

def _save_to_remote(db_manager, file_name: str, file_timestamp: float, 
                   g_Daten, g_LageZuordnung, g_PaketPos, 
                   g_PaketeZuordnung, g_Zwischenlagen,
                   g_paket_quer, g_CenterOfGravity, g_PalettenDim,
                   g_PaketDim, g_LageArten, g_AnzLagen, g_AnzahlPakete):
    """Save data to remote PostgreSQL database."""
    if not db_manager.is_online():
        return
    
    # Ensure remote schema exists
    create_remote_database(db_manager)
    
    remote_conn = db_manager.remote_pool.getconn()
    try:
        remote_cursor = remote_conn.cursor()
        
        # Check if exists
        remote_cursor.execute('''
            SELECT id, file_timestamp FROM paletten_metadata 
            WHERE file_name = %s
        ''', (file_name,))
        existing = remote_cursor.fetchone()
        
        if existing:
            existing_id, existing_ts = existing
            if existing_ts and file_timestamp and existing_ts >= file_timestamp:
                # Remote is newer, skip
                return
            # Delete existing
            remote_cursor.execute("DELETE FROM paletten_metadata WHERE id = %s", (existing_id,))
        
        # Insert metadata with PostgreSQL syntax
        remote_cursor.execute('''
            INSERT INTO paletten_metadata (
                paket_quer, center_of_gravity_x, center_of_gravity_y,
                center_of_gravity_z, lage_arten, anz_lagen, anzahl_pakete,
                file_timestamp, file_name, sync_status, last_modified
            ) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, 'synced', EXTRACT(EPOCH FROM NOW()))
            RETURNING id
        ''', (g_paket_quer, g_CenterOfGravity[0], g_CenterOfGravity[1],
              g_CenterOfGravity[2], g_LageArten, g_AnzLagen,
              g_AnzahlPakete, file_timestamp, file_name))
        
        remote_metadata_id = remote_cursor.fetchone()[0]
        
        # Save related data
        for i, row in enumerate(g_Daten):
            for j, value in enumerate(row):
                remote_cursor.execute('''
                INSERT INTO daten (metadata_id, row_index, col_index, value) 
                VALUES (%s, %s, %s, %s)
                ''', (remote_metadata_id, i, j, value))
        
        if g_PalettenDim:
            remote_cursor.execute('''
            INSERT INTO paletten_dim (metadata_id, length, width, height) 
            VALUES (%s, %s, %s, %s)
            ''', (remote_metadata_id, g_PalettenDim[0], g_PalettenDim[1], g_PalettenDim[2]))
        
        if g_PaketDim:
            remote_cursor.execute('''
            INSERT INTO paket_dim (metadata_id, length, width, height, gap, weight) 
            VALUES (%s, %s, %s, %s, %s, %s)
            ''', (remote_metadata_id, g_PaketDim[0], g_PaketDim[1], 
                  g_PaketDim[2], g_PaketDim[3], None))
        
        for i, value in enumerate(g_LageZuordnung):
            remote_cursor.execute('''
            INSERT INTO lage_zuordnung (metadata_id, lage_index, value) 
            VALUES (%s, %s, %s)
            ''', (remote_metadata_id, i, value))
        
        for i, value in enumerate(g_Zwischenlagen):
            remote_cursor.execute('''
            INSERT INTO zwischenlagen (metadata_id, lage_index, value) 
            VALUES (%s, %s, %s)
            ''', (remote_metadata_id, i, value))
        
        for i, value in enumerate(g_PaketeZuordnung):
            remote_cursor.execute('''
            INSERT INTO pakete_zuordnung (metadata_id, lage_index, value) 
            VALUES (%s, %s, %s)
            ''', (remote_metadata_id, i, value))
        
        for i, pos in enumerate(g_PaketPos):
            remote_cursor.execute('''
            INSERT INTO paket_pos (metadata_id, paket_index, xp, yp, ap, xd, yd, ad, nop, xvec, yvec) 
            VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            ''', (remote_metadata_id, i, pos[0], pos[1], pos[2], pos[3], pos[4], pos[5], pos[6], pos[7], pos[8]))
        
        remote_conn.commit()
    except Exception as e:
        remote_conn.rollback()
        raise e
    finally:
        db_manager.return_connection(remote_conn, 'remote')
    for i, row in enumerate(g_Daten):
        for j, value in enumerate(row):
            cursor.execute('''
            INSERT INTO daten (metadata_id, row_index, col_index, value) 
            VALUES (?, ?, ?, ?)
            ''', (metadata_id, i, j, value))
    
    # Save pallet dimensions if available
    if g_PalettenDim:
        cursor.execute('''
        INSERT INTO paletten_dim (metadata_id, length, width, height) 
        VALUES (?, ?, ?, ?)
        ''', (metadata_id, g_PalettenDim[0], g_PalettenDim[1], 
              g_PalettenDim[2]))
    
    # Save package dimensions if available
    if g_PaketDim:
        cursor.execute('''
        INSERT INTO paket_dim (metadata_id, length, width, height, gap, weight) 
        VALUES (?, ?, ?, ?, ?, ?)
        ''', (metadata_id, g_PaketDim[0], g_PaketDim[1], 
              g_PaketDim[2], g_PaketDim[3], None))
    
    # Save layer type assignments (which type is each layer)
    for i, value in enumerate(g_LageZuordnung):
        cursor.execute('''
        INSERT INTO lage_zuordnung (metadata_id, lage_index, value) 
        VALUES (?, ?, ?)
        ''', (metadata_id, i, value))
    
    # Save intermediate layer flags (whether each layer has separator)
    for i, value in enumerate(g_Zwischenlagen):
        cursor.execute('''
        INSERT INTO zwischenlagen (metadata_id, lage_index, value) 
        VALUES (?, ?, ?)
        ''', (metadata_id, i, value))
    
    # Save number of packages per layer type
    for i, value in enumerate(g_PaketeZuordnung):
        cursor.execute('''
        INSERT INTO pakete_zuordnung (metadata_id, lage_index, value) 
        VALUES (?, ?, ?)
        ''', (metadata_id, i, value))
    
    # Save package positions with pick/place coordinates and angles
    for i, pos in enumerate(g_PaketPos):
        cursor.execute('''
        INSERT INTO paket_pos (metadata_id, paket_index, xp, yp, ap, xd, yd, ad, nop, xvec, yvec) 
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ''', (metadata_id, i, pos[0], pos[1], pos[2], pos[3], pos[4], pos[5], pos[6], pos[7], pos[8]))
    
    logger.info(f"Saved file: {file_name} to database")
    conn.commit()
    conn.close()
    return True

def load_from_database(db_path="paletten.db", file_name=None, metadata_id=None, db_manager=None) -> Union[Literal[0], Literal[1]]:
    """Load all data from the database to global variables.
    
    Args:
        db_path (str): Path to the database (deprecated if db_manager provided)
        file_name (str, optional): Specific .rob filename to load. If None, loads the most recent entry.
        metadata_id (int, optional): Specific metadata ID to load. Takes precedence over file_name.
        db_manager: Optional HybridDatabaseManager instance for hybrid sync
        
    Returns:
        Union[Literal[0], Literal[1]]: 0 if successful, 1 otherwise.
    """
    # Use db_manager if provided, otherwise use legacy SQLite-only mode
    if db_manager:
        # Try remote first if online, fallback to local
        if db_manager.is_online():
            try:
                result = _load_from_remote(db_manager, file_name, metadata_id)
                if result != 1:  # Success
                    return result
            except Exception as e:
                logger.warning(f"Failed to load from remote, using local: {e}")
        
        # Fallback to local
        return _load_from_local(db_manager.local_db_path, file_name, metadata_id)
    else:
        return _load_from_local(db_path, file_name, metadata_id)

def _load_from_local(db_path, file_name=None, metadata_id=None):
    """Load from local SQLite database."""
    try:
        # Initialize all globals to empty/default values
        g_Daten = []                  # 2D array containing all data from .rob file
        g_LageZuordnung = []         # Layer assignments (which layer type for each layer)
        g_PaketPos = []              # Package positions for each layer
        g_PaketeZuordnung = []       # Package assignments for each layer
        g_Zwischenlagen = []         # Intermediate layers between package layers
        g_paket_quer = 1             # Package orientation (1 = default)
        g_CenterOfGravity = [0,0,0]  # Center of gravity coordinates [x,y,z]
        g_PalettenDim = []           # Pallet dimensions [length,width,height]
        g_PaketDim = []              # Package dimensions [length,width,height,gap]
        
        # Connect to database
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        
        # Enable foreign key support for data integrity
        cursor.execute("PRAGMA foreign_keys = ON")
        
        # Find the metadata entry to load based on provided criteria
        if metadata_id is not None:
            # If specific metadata ID is provided, use it directly
            cursor.execute('''
            SELECT id, paket_quer, center_of_gravity_x, center_of_gravity_y, center_of_gravity_z, 
                   lage_arten, anz_lagen, anzahl_pakete, file_timestamp, file_name 
            FROM paletten_metadata 
            WHERE id = ?
            ''', (metadata_id,))
            result = cursor.fetchone()
            if not result:
                logger.error(f"Metadata ID {metadata_id} not found in database")
                return 1
        elif file_name:
            # If a specific file is requested, search for it by name
            cursor.execute('''
            SELECT id, paket_quer, center_of_gravity_x, center_of_gravity_y, center_of_gravity_z, 
                   lage_arten, anz_lagen, anzahl_pakete, file_timestamp, file_name 
            FROM paletten_metadata 
            WHERE file_name LIKE ?
            ''', (f"%{file_name}%",))
            result = cursor.fetchone()
            if not result:
                logger.error(f"File '{file_name}' not found in database")
                return 1
        else:
            # Otherwise load the most recent entry
            cursor.execute('''
            SELECT id, paket_quer, center_of_gravity_x, center_of_gravity_y, center_of_gravity_z, 
                   lage_arten, anz_lagen, anzahl_pakete, file_timestamp, file_name 
            FROM paletten_metadata 
            ORDER BY file_timestamp DESC
            LIMIT 1
            ''')
            result = cursor.fetchone()
            if not result:
                logger.error("No data found in database")
                return 1
        
        # Extract metadata from query result
        metadata_id = result[0]                                   # Unique ID for this dataset
        g_paket_quer = result[1]                     # Package orientation
        g_CenterOfGravity = [result[2], result[3], result[4]]   # Center of gravity [x,y,z]
        g_LageArten = result[5]                      # Number of layer types
        g_AnzLagen = result[6]                       # Total number of layers
        g_AnzahlPakete = result[7]                   # Total number of packages
        file_timestamp = result[8]                               # When file was last modified
        file_name = result[9]                                    # Original .rob filename
        
        # Print info about the data being loaded
        if file_timestamp:
            timestamp_str = datetime.datetime.fromtimestamp(file_timestamp).strftime('%Y-%m-%d %H:%M:%S')
            logger.info(f"Loading data ID: {metadata_id}")
            logger.info(f"Data from file: {file_name}")
            logger.info(f"Last modified: {timestamp_str}")
        
        # Load g_Daten - the main 2D data array from the .rob file
        cursor.execute('''
        SELECT row_index, col_index, value FROM daten 
        WHERE metadata_id = ?
        ORDER BY row_index, col_index
        ''', (metadata_id,))
        results = cursor.fetchall()
        
        # Create the 2D structure for g_Daten
        max_row = max([r[0] for r in results]) if results else -1
        for i in range(max_row + 1):
            g_Daten.append([])
        
        # Populate g_Daten with values from database
        for row_idx, col_idx, value in results:
            # Extend each row as needed to accommodate column index
            while len(g_Daten[row_idx]) <= col_idx:
                g_Daten[row_idx].append(0)
            g_Daten[row_idx][col_idx] = value
        
        # Load pallet dimensions [length,width,height]
        cursor.execute('''
        SELECT length, width, height FROM paletten_dim 
        WHERE metadata_id = ?
        ''', (metadata_id,))
        result = cursor.fetchone()
        if result:
            g_PalettenDim = list(result)
        
        # Load package dimensions [length,width,height,gap] and weight
        cursor.execute('''
        SELECT length, width, height, gap, weight FROM paket_dim 
        WHERE metadata_id = ?
        ''', (metadata_id,))
        result = cursor.fetchone()
        g_BoxWeight = None
        if result:
            g_PaketDim = list(result[:4])  # Only first 4 values for dimensions
            g_BoxWeight = result[4] if len(result) > 4 else None
        
        # Load layer type assignments
        cursor.execute('''
        SELECT value FROM lage_zuordnung
        WHERE metadata_id = ?
        ORDER BY lage_index
        ''', (metadata_id,))
        g_LageZuordnung = [r[0] for r in cursor.fetchall()]
        
        # Load intermediate layer flags
        cursor.execute('''
        SELECT value FROM zwischenlagen
        WHERE metadata_id = ?
        ORDER BY lage_index
        ''', (metadata_id,))
        g_Zwischenlagen = [r[0] for r in cursor.fetchall()]
        
        # Load package assignments for each layer
        cursor.execute('''
        SELECT value FROM pakete_zuordnung
        WHERE metadata_id = ?
        ORDER BY lage_index
        ''', (metadata_id,))
        g_PaketeZuordnung = [r[0] for r in cursor.fetchall()]
        
        # Load package positions [xp,yp,ap,xd,yd,ad,nop,xvec,yvec]
        cursor.execute('''
        SELECT xp, yp, ap, xd, yd, ad, nop, xvec, yvec FROM paket_pos
        WHERE metadata_id = ?
        ORDER BY paket_index
        ''', (metadata_id,))
        g_PaketPos = [list(r) for r in cursor.fetchall()]
        
        conn.close()
        
        return g_Daten, g_LageZuordnung, g_PaketPos, g_PaketeZuordnung, g_Zwischenlagen, g_paket_quer, g_CenterOfGravity, g_PalettenDim, g_PaketDim, g_LageArten, g_AnzLagen, g_AnzahlPakete, g_BoxWeight
    except Exception as e:
        logger.error(f"Error loading data from database: {e}")
        return 1

def _load_from_remote(db_manager, file_name=None, metadata_id=None):
    """Load from remote PostgreSQL database."""
    try:
        remote_conn = db_manager.remote_pool.getconn()
        remote_cursor = remote_conn.cursor()
        
        # Find metadata entry
        if metadata_id is not None:
            remote_cursor.execute('''
            SELECT id, paket_quer, center_of_gravity_x, center_of_gravity_y, center_of_gravity_z, 
                   lage_arten, anz_lagen, anzahl_pakete, file_timestamp, file_name 
            FROM paletten_metadata 
            WHERE id = %s
            ''', (metadata_id,))
            result = remote_cursor.fetchone()
            if not result:
                db_manager.return_connection(remote_conn, 'remote')
                return 1
        elif file_name:
            remote_cursor.execute('''
            SELECT id, paket_quer, center_of_gravity_x, center_of_gravity_y, center_of_gravity_z, 
                   lage_arten, anz_lagen, anzahl_pakete, file_timestamp, file_name 
            FROM paletten_metadata 
            WHERE file_name LIKE %s
            ''', (f"%{file_name}%",))
            result = remote_cursor.fetchone()
            if not result:
                db_manager.return_connection(remote_conn, 'remote')
                return 1
        else:
            remote_cursor.execute('''
            SELECT id, paket_quer, center_of_gravity_x, center_of_gravity_y, center_of_gravity_z, 
                   lage_arten, anz_lagen, anzahl_pakete, file_timestamp, file_name 
            FROM paletten_metadata 
            ORDER BY file_timestamp DESC
            LIMIT 1
            ''')
            result = remote_cursor.fetchone()
            if not result:
                db_manager.return_connection(remote_conn, 'remote')
                return 1
        
        metadata_id = result[0]
        g_paket_quer = result[1]
        g_CenterOfGravity = [result[2], result[3], result[4]]
        g_LageArten = result[5]
        g_AnzLagen = result[6]
        g_AnzahlPakete = result[7]
        file_timestamp = result[8]
        file_name = result[9]
        
        # Load g_Daten
        remote_cursor.execute('''
        SELECT row_index, col_index, value FROM daten 
        WHERE metadata_id = %s
        ORDER BY row_index, col_index
        ''', (metadata_id,))
        results = remote_cursor.fetchall()
        
        g_Daten = []
        max_row = max([r[0] for r in results]) if results else -1
        for i in range(max_row + 1):
            g_Daten.append([])
        
        for row_idx, col_idx, value in results:
            while len(g_Daten[row_idx]) <= col_idx:
                g_Daten[row_idx].append(0)
            g_Daten[row_idx][col_idx] = value
        
        # Load other data (similar to _load_from_local but with %s placeholders)
        remote_cursor.execute('''
        SELECT length, width, height FROM paletten_dim 
        WHERE metadata_id = %s
        ''', (metadata_id,))
        result = remote_cursor.fetchone()
        g_PalettenDim = list(result) if result else []
        
        remote_cursor.execute('''
        SELECT length, width, height, gap, weight FROM paket_dim 
        WHERE metadata_id = %s
        ''', (metadata_id,))
        result = remote_cursor.fetchone()
        g_BoxWeight = None
        if result:
            g_PaketDim = list(result[:4])
            g_BoxWeight = result[4] if len(result) > 4 else None
        else:
            g_PaketDim = []
        
        remote_cursor.execute('''
        SELECT value FROM lage_zuordnung
        WHERE metadata_id = %s
        ORDER BY lage_index
        ''', (metadata_id,))
        g_LageZuordnung = [r[0] for r in remote_cursor.fetchall()]
        
        remote_cursor.execute('''
        SELECT value FROM zwischenlagen
        WHERE metadata_id = %s
        ORDER BY lage_index
        ''', (metadata_id,))
        g_Zwischenlagen = [r[0] for r in remote_cursor.fetchall()]
        
        remote_cursor.execute('''
        SELECT value FROM pakete_zuordnung
        WHERE metadata_id = %s
        ORDER BY lage_index
        ''', (metadata_id,))
        g_PaketeZuordnung = [r[0] for r in remote_cursor.fetchall()]
        
        remote_cursor.execute('''
        SELECT xp, yp, ap, xd, yd, ad, nop, xvec, yvec FROM paket_pos
        WHERE metadata_id = %s
        ORDER BY paket_index
        ''', (metadata_id,))
        g_PaketPos = [list(r) for r in remote_cursor.fetchall()]
        
        db_manager.return_connection(remote_conn, 'remote')
        
        return g_Daten, g_LageZuordnung, g_PaketPos, g_PaketeZuordnung, g_Zwischenlagen, g_paket_quer, g_CenterOfGravity, g_PalettenDim, g_PaketDim, g_LageArten, g_AnzLagen, g_AnzahlPakete, g_BoxWeight
    except Exception as e:
        logger.error(f"Error loading data from remote database: {e}")
        return 1

def sync_local_to_remote(db_manager) -> bool:
    """Sync all local pending changes to remote database.
    """
    if not db_manager or not db_manager.is_online():
        logger.info("Sync: Skipping local->remote sync: remote database currently offline or db_manager missing")
        return False
    with db_manager.sync_lock:
        logger.info("Sync: Starting local->remote sync")
        local_conn = None
        remote_conn = None
        try:
            # Open local connection
            local_conn = sqlite3.connect(db_manager.local_db_path)
            local_cursor = local_conn.cursor()
            local_cursor.execute("PRAGMA foreign_keys = ON")

            # Fetch all metadata rows that need syncing
            local_cursor.execute("""
                SELECT id, file_name, file_timestamp, sync_status, sync_timestamp
                FROM paletten_metadata
                WHERE sync_status IN ('pending', 'modified') OR sync_status IS NULL
                ORDER BY file_timestamp DESC
            """)
            pending_items = local_cursor.fetchall()
            if not pending_items:
                logger.info("Sync: No pending items to sync to remote database")
                return True
            logger.info("Sync: Syncing %s items to remote database", len(pending_items))

            # Ensure remote schema exists and get remote connection
            create_remote_database(db_manager)
            remote_conn = db_manager.remote_pool.getconn()
            remote_cursor = remote_conn.cursor()
            synced_count = 0

            # ... (existing per-item sync logic with detailed logging) ...

            for local_metadata_id, file_name, file_timestamp, sync_status, sync_timestamp in pending_items:
                logger.debug(
                    "Syncing metadata_id=%s file_name=%s status=%s",
                    local_metadata_id,
                    file_name,
                    sync_status,
                )
                try:
                    # Load full dataset for this metadata_id from local SQLite
                    # 1) Paletten metadata
                    local_cursor.execute('''
                        SELECT paket_quer, center_of_gravity_x, center_of_gravity_y,
                               center_of_gravity_z, lage_arten, anz_lagen, anzahl_pakete,
                               file_timestamp, file_name, last_modified
                        FROM paletten_metadata
                        WHERE id = ?
                    ''', (local_metadata_id,))
                    meta_row = local_cursor.fetchone()
                    if not meta_row:
                        logger.warning(f"Skipping metadata_id={local_metadata_id}: not found in local DB")
                        continue

                    (paket_quer, cog_x, cog_y, cog_z,
                     lage_arten, anz_lagen, anzahl_pakete,
                     file_ts, file_name_local, last_modified) = meta_row

                    # 2) Raw daten rows
                    local_cursor.execute('''
                        SELECT row_index, col_index, value
                        FROM daten
                        WHERE metadata_id = ?
                        ORDER BY row_index, col_index
                    ''', (local_metadata_id,))
                    daten_rows = local_cursor.fetchall()

                    # 3) paletten_dim
                    local_cursor.execute('''
                        SELECT length, width, height
                        FROM paletten_dim
                        WHERE metadata_id = ?
                    ''', (local_metadata_id,))
                    paletten_dim_row = local_cursor.fetchone()

                    # 4) paket_dim
                    local_cursor.execute('''
                        SELECT length, width, height, gap, weight, einzelpaket_laengs
                        FROM paket_dim
                        WHERE metadata_id = ?
                    ''', (local_metadata_id,))
                    paket_dim_row = local_cursor.fetchone()

                    # 5) lage_zuordnung
                    local_cursor.execute('''
                        SELECT lage_index, value
                        FROM lage_zuordnung
                        WHERE metadata_id = ?
                        ORDER BY lage_index
                    ''', (local_metadata_id,))
                    lage_zuordnung_rows = local_cursor.fetchall()

                    # 6) zwischenlagen
                    local_cursor.execute('''
                        SELECT lage_index, value
                        FROM zwischenlagen
                        WHERE metadata_id = ?
                        ORDER BY lage_index
                    ''', (local_metadata_id,))
                    zwischenlagen_rows = local_cursor.fetchall()

                    # 7) pakete_zuordnung
                    local_cursor.execute('''
                        SELECT lage_index, value
                        FROM pakete_zuordnung
                        WHERE metadata_id = ?
                        ORDER BY lage_index
                    ''', (local_metadata_id,))
                    pakete_zuordnung_rows = local_cursor.fetchall()

                    # 8) paket_pos
                    local_cursor.execute('''
                        SELECT paket_index, xp, yp, ap, xd, yd, ad, nop, xvec, yvec
                        FROM paket_pos
                        WHERE metadata_id = ?
                        ORDER BY paket_index
                    ''', (local_metadata_id,))
                    paket_pos_rows = local_cursor.fetchall()

                    # Upsert into remote:
                    # First check if remote already has this file_name and if it is newer
                    remote_cursor.execute('''
                        SELECT id, file_timestamp
                        FROM paletten_metadata
                        WHERE file_name = %s
                    ''', (file_name_local,))
                    existing_remote = remote_cursor.fetchone()

                    if existing_remote:
                        remote_id, remote_ts = existing_remote
                        if remote_ts is not None and file_ts is not None and remote_ts >= file_ts:
                            logger.info(
                                "Skipping sync for file %s: remote is newer or same age",
                                file_name_local,
                            )
                            continue
                        # Delete existing remote record (CASCADE will clear children)
                        remote_cursor.execute(
                            "DELETE FROM paletten_metadata WHERE id = %s",
                            (remote_id,),
                        )

                    # Insert metadata in remote
                    remote_cursor.execute('''
                        INSERT INTO paletten_metadata (
                            paket_quer, center_of_gravity_x, center_of_gravity_y,
                            center_of_gravity_z, lage_arten, anz_lagen, anzahl_pakete,
                            file_timestamp, file_name, sync_status, sync_timestamp, last_modified
                        ) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s,
                                  'synced', EXTRACT(EPOCH FROM NOW()), %s)
                        RETURNING id
                    ''', (
                        paket_quer, cog_x, cog_y, cog_z,
                        lage_arten, anz_lagen, anzahl_pakete,
                        file_ts, file_name_local, last_modified or time.time(),
                    ))
                    remote_metadata_id = remote_cursor.fetchone()[0]

                    # Insert daten rows
                    for row_index, col_index, value in daten_rows:
                        remote_cursor.execute('''
                            INSERT INTO daten (metadata_id, row_index, col_index, value)
                            VALUES (%s, %s, %s, %s)
                        ''', (remote_metadata_id, row_index, col_index, value))

                    # Insert paletten_dim
                    if paletten_dim_row:
                        length, width, height = paletten_dim_row
                        remote_cursor.execute('''
                            INSERT INTO paletten_dim (metadata_id, length, width, height)
                            VALUES (%s, %s, %s, %s)
                        ''', (remote_metadata_id, length, width, height))

                    # Insert paket_dim
                    if paket_dim_row:
                        length, width, height, gap, weight, einzelpaket_laengs = paket_dim_row
                        remote_cursor.execute('''
                            INSERT INTO paket_dim (
                                metadata_id, length, width, height, gap, weight, einzelpaket_laengs
                            )
                            VALUES (%s, %s, %s, %s, %s, %s, %s)
                        ''', (
                            remote_metadata_id, length, width, height, gap, weight, einzelpaket_laengs,
                        ))

                    # Insert lage_zuordnung
                    for lage_index, value in lage_zuordnung_rows:
                        remote_cursor.execute('''
                            INSERT INTO lage_zuordnung (metadata_id, lage_index, value)
                            VALUES (%s, %s, %s)
                        ''', (remote_metadata_id, lage_index, value))

                    # Insert zwischenlagen
                    for lage_index, value in zwischenlagen_rows:
                        remote_cursor.execute('''
                            INSERT INTO zwischenlagen (metadata_id, lage_index, value)
                            VALUES (%s, %s, %s)
                        ''', (remote_metadata_id, lage_index, value))

                    # Insert pakete_zuordnung
                    for lage_index, value in pakete_zuordnung_rows:
                        remote_cursor.execute('''
                            INSERT INTO pakete_zuordnung (metadata_id, lage_index, value)
                            VALUES (%s, %s, %s)
                        ''', (remote_metadata_id, lage_index, value))

                    # Insert paket_pos
                    for (paket_index, xp, yp, ap, xd, yd, ad,
                         nop, xvec, yvec) in paket_pos_rows:
                        remote_cursor.execute('''
                            INSERT INTO paket_pos (
                                metadata_id, paket_index, xp, yp, ap, xd, yd, ad,
                                nop, xvec, yvec
                            )
                            VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
                        ''', (
                            remote_metadata_id, paket_index, xp, yp, ap, xd, yd,
                            ad, nop, xvec, yvec,
                        ))

                    # Mark local row as synced
                    local_cursor.execute('''
                        UPDATE paletten_metadata
                        SET sync_status = 'synced',
                            sync_timestamp = ?,
                            last_modified = ?
                        WHERE id = ?
                    ''', (time.time(), last_modified or time.time(), local_metadata_id))

                    synced_count += 1
                    logger.info("Successfully synced file %s (metadata_id=%s)", file_name_local, local_metadata_id)
                except Exception as item_error:
                    logger.error(
                        "Error syncing item metadata_id=%s file_name=%s: %s",
                        local_metadata_id,
                        file_name,
                        item_error,
                    )

            # Commit both sides
            remote_conn.commit()
            local_conn.commit()

            logger.info("Synced %s/%s items successfully", synced_count, len(pending_items))
            db_manager.last_sync_time = time.time()
            return True
        except Exception as e:
            logger.error(f"Sync: Error during local->remote sync: {e}")
            if remote_conn:
                remote_conn.rollback()
            if local_conn:
                local_conn.rollback()
            return False
        finally:
            if remote_conn:
                db_manager.return_connection(remote_conn, 'remote')
            if local_conn:
                local_conn.close()


def sync_remote_to_local(db_manager) -> bool:
    """Sync newer records from remote PostgreSQL back to local SQLite (last-write-wins).

    This is a simple, optional helper that can be used on startup or on demand to
    ensure that plans created on another machine appear locally as well.
    """
    if not db_manager or not db_manager.is_online():
        logger.info("Skipping remote->local sync: remote database currently offline or db_manager missing")
        return False

    with db_manager.sync_lock:
        logger.info("Starting remote->local sync")
        local_conn = None
        remote_conn = None
        try:
            # Open connections
            local_conn = sqlite3.connect(db_manager.local_db_path)
            local_cursor = local_conn.cursor()
            local_cursor.execute("PRAGMA foreign_keys = ON")

            remote_conn = db_manager.remote_pool.getconn()
            remote_cursor = remote_conn.cursor()

            # Build a map of local files and their last_modified timestamp
            local_cursor.execute('''
                SELECT file_name, last_modified
                FROM paletten_metadata
                WHERE file_name IS NOT NULL
            ''')
            local_rows = local_cursor.fetchall()
            local_index = {fn: lm for (fn, lm) in local_rows if fn}

            # Fetch all remote metadata rows (for now keep it simple)
            remote_cursor.execute('''
                SELECT id, file_name, file_timestamp, last_modified
                FROM paletten_metadata
                WHERE file_name IS NOT NULL
            ''')
            remote_meta_rows = remote_cursor.fetchall()

            if not remote_meta_rows:
                logger.info("No remote records found for remote->local sync")
                return True

            synced_count = 0

            for remote_id, file_name, file_ts, remote_last_modified in remote_meta_rows:
                if not file_name:
                    continue

                local_last_modified = local_index.get(file_name)
                # Last-write-wins: only pull if remote is newer or local missing
                if local_last_modified is not None and remote_last_modified is not None:
                    if local_last_modified >= remote_last_modified:
                        logger.debug(
                            "Skipping remote->local for %s (local is newer or same)",
                            file_name,
                        )
                        continue

                logger.info(
                    "Remote->local: importing file %s (remote_last_modified=%s, local_last_modified=%s)",
                    file_name,
                    remote_last_modified,
                    local_last_modified,
                )

                # Load full dataset for this remote_id
                # 1) metadata
                remote_cursor.execute('''
                    SELECT paket_quer, center_of_gravity_x, center_of_gravity_y,
                           center_of_gravity_z, lage_arten, anz_lagen, anzahl_pakete,
                           file_timestamp, file_name, sync_status, sync_timestamp, last_modified
                    FROM paletten_metadata
                    WHERE id = %s
                ''', (remote_id,))
                meta_row = remote_cursor.fetchone()
                if not meta_row:
                    logger.warning("Remote->local: metadata id %s not found, skipping", remote_id)
                    continue

                (paket_quer, cog_x, cog_y, cog_z,
                 lage_arten, anz_lagen, anzahl_pakete,
                 file_ts_full, file_name_full, sync_status,
                 sync_timestamp, last_modified_full) = meta_row

                # 2) daten
                remote_cursor.execute('''
                    SELECT row_index, col_index, value
                    FROM daten
                    WHERE metadata_id = %s
                    ORDER BY row_index, col_index
                ''', (remote_id,))
                daten_rows = remote_cursor.fetchall()

                # 3) paletten_dim
                remote_cursor.execute('''
                    SELECT length, width, height
                    FROM paletten_dim
                    WHERE metadata_id = %s
                ''', (remote_id,))
                paletten_dim_row = remote_cursor.fetchone()

                # 4) paket_dim
                remote_cursor.execute('''
                    SELECT length, width, height, gap, weight, einzelpaket_laengs
                    FROM paket_dim
                    WHERE metadata_id = %s
                ''', (remote_id,))
                paket_dim_row = remote_cursor.fetchone()

                # 5) lage_zuordnung
                remote_cursor.execute('''
                    SELECT lage_index, value
                    FROM lage_zuordnung
                    WHERE metadata_id = %s
                    ORDER BY lage_index
                ''', (remote_id,))
                lage_zuordnung_rows = remote_cursor.fetchall()

                # 6) zwischenlagen
                remote_cursor.execute('''
                    SELECT lage_index, value
                    FROM zwischenlagen
                    WHERE metadata_id = %s
                    ORDER BY lage_index
                ''', (remote_id,))
                zwischenlagen_rows = remote_cursor.fetchall()

                # 7) pakete_zuordnung
                remote_cursor.execute('''
                    SELECT lage_index, value
                    FROM pakete_zuordnung
                    WHERE metadata_id = %s
                    ORDER BY lage_index
                ''', (remote_id,))
                pakete_zuordnung_rows = remote_cursor.fetchall()

                # 8) paket_pos
                remote_cursor.execute('''
                    SELECT paket_index, xp, yp, ap, xd, yd, ad, nop, xvec, yvec
                    FROM paket_pos
                    WHERE metadata_id = %s
                    ORDER BY paket_index
                ''', (remote_id,))
                paket_pos_rows = remote_cursor.fetchall()

                # Remove existing local record for this file_name to avoid duplicates
                local_cursor.execute(
                    "DELETE FROM paletten_metadata WHERE file_name = ?",
                    (file_name_full,),
                )

                # Insert new metadata row locally
                local_cursor.execute('''
                    INSERT INTO paletten_metadata (
                        paket_quer, center_of_gravity_x, center_of_gravity_y,
                        center_of_gravity_z, lage_arten, anz_lagen, anzahl_pakete,
                        file_timestamp, file_name, sync_status, sync_timestamp, last_modified
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                ''', (
                    paket_quer, cog_x, cog_y, cog_z,
                    lage_arten, anz_lagen, anzahl_pakete,
                    file_ts_full, file_name_full,
                    sync_status or 'synced',
                    sync_timestamp,
                    last_modified_full or time.time(),
                ))
                new_local_id = local_cursor.lastrowid

                # Insert daten
                for row_index, col_index, value in daten_rows:
                    local_cursor.execute('''
                        INSERT INTO daten (metadata_id, row_index, col_index, value)
                        VALUES (?, ?, ?, ?)
                    ''', (new_local_id, row_index, col_index, value))

                # Insert paletten_dim
                if paletten_dim_row:
                    length, width, height = paletten_dim_row
                    local_cursor.execute('''
                        INSERT INTO paletten_dim (metadata_id, length, width, height)
                        VALUES (?, ?, ?, ?)
                    ''', (new_local_id, length, width, height))

                # Insert paket_dim
                if paket_dim_row:
                    length, width, height, gap, weight, einzelpaket_laengs = paket_dim_row
                    local_cursor.execute('''
                        INSERT INTO paket_dim (
                            metadata_id, length, width, height, gap, weight, einzelpaket_laengs
                        )
                        VALUES (?, ?, ?, ?, ?, ?, ?)
                    ''', (
                        new_local_id, length, width, height, gap, weight, einzelpaket_laengs,
                    ))

                # Insert lage_zuordnung
                for lage_index, value in lage_zuordnung_rows:
                    local_cursor.execute('''
                        INSERT INTO lage_zuordnung (metadata_id, lage_index, value)
                        VALUES (?, ?, ?)
                    ''', (new_local_id, lage_index, value))

                # Insert zwischenlagen
                for lage_index, value in zwischenlagen_rows:
                    local_cursor.execute('''
                        INSERT INTO zwischenlagen (metadata_id, lage_index, value)
                        VALUES (?, ?, ?)
                    ''', (new_local_id, lage_index, value))

                # Insert pakete_zuordnung
                for lage_index, value in pakete_zuordnung_rows:
                    local_cursor.execute('''
                        INSERT INTO pakete_zuordnung (metadata_id, lage_index, value)
                        VALUES (?, ?, ?)
                    ''', (new_local_id, lage_index, value))

                # Insert paket_pos
                for (paket_index, xp, yp, ap, xd, yd, ad,
                     nop, xvec, yvec) in paket_pos_rows:
                    local_cursor.execute('''
                        INSERT INTO paket_pos (
                            metadata_id, paket_index, xp, yp, ap, xd, yd, ad,
                            nop, xvec, yvec
                        )
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                    ''', (
                        new_local_id, paket_index, xp, yp, ap, xd, yd,
                        ad, nop, xvec, yvec,
                    ))

                synced_count += 1

            remote_conn.commit()
            local_conn.commit()
            logger.info("Remote->local sync completed, imported %s items", synced_count)
            return True
        except Exception as e:
            logger.error(f"Error during remote->local sync: {e}")
            if remote_conn:
                remote_conn.rollback()
            if local_conn:
                local_conn.rollback()
            return False
        finally:
            if remote_conn:
                db_manager.return_connection(remote_conn, 'remote')
            if local_conn:
                local_conn.close()

def get_sync_status(db_manager) -> Dict[str, Any]:
    """Get sync status information.
    
    Args:
        db_manager: HybridDatabaseManager instance
        
    Returns:
        dict with sync status information
    """
    if not db_manager:
        return {"online": False, "pending_count": 0}
    
    try:
        local_conn = sqlite3.connect(db_manager.local_db_path)
        local_cursor = local_conn.cursor()
        
        local_cursor.execute('''
            SELECT COUNT(*) FROM paletten_metadata
            WHERE sync_status IN ('pending', 'modified') OR sync_status IS NULL
        ''')
        pending_count = local_cursor.fetchone()[0]
        
        local_conn.close()
        
        return {
            "online": db_manager.is_online(),
            "pending_count": pending_count,
            "last_sync": db_manager.last_sync_time
        }
    except Exception as e:
        logger.error(f"Error getting sync status: {e}")
        return {"online": False, "pending_count": 0}

def list_available_files(db_path="paletten.db") -> List[Dict[str, Any]]:
    """List all .rob files stored in the database.
    
    Returns:
        List[Dict[str, Any]]: A list of dictionaries containing file info (name, timestamp)
    """
    try:
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
        SELECT id, file_name, file_timestamp FROM paletten_metadata 
        WHERE file_name IS NOT NULL AND file_name LIKE '%.rob'
        ORDER BY file_timestamp DESC
        ''')
        
        results = cursor.fetchall()
        files = []
        
        for metadata_id, file_name, timestamp in results:
            timestamp_str = datetime.datetime.fromtimestamp(timestamp).strftime('%Y-%m-%d %H:%M:%S')
            files.append({
                "id": metadata_id,
                "file_name": file_name,
                "timestamp": timestamp,
                "timestamp_str": timestamp_str
            })
        
        conn.close()
        return files
    except Exception as e:
        logger.error(f"Error listing files from database: {e}")
        return []

def find_file_in_database(file_name: str, db_path="paletten.db") -> Optional[Dict[str, Any]]:
    """Find a specific .rob file in the database.
    
    Args:
        file_name (str): Name of the file to find
        db_path (str): Path to the database
        
    Returns:
        Optional[Dict[str, Any]]: File info or None if not found
    """
    try:
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
        SELECT id, file_name, file_timestamp FROM paletten_metadata 
        WHERE file_name LIKE ?
        ''', (f"%{file_name}%",))
        
        result = cursor.fetchone()
        if result:
            metadata_id, file_name, timestamp = result
            timestamp_str = datetime.datetime.fromtimestamp(timestamp).strftime('%Y-%m-%d %H:%M:%S')
            conn.close()
            return {
                "id": metadata_id,
                "file_name": file_name,
                "timestamp": timestamp,
                "timestamp_str": timestamp_str
            }
        
        conn.close()
        return None
    except Exception as e:
        logger.error(f"Error searching for file in database: {e}")
        return None 
    
def find_palettplan(package_length=0, package_width=0, package_height=0, db_path="paletten.db") -> Optional[List[str]]:
    """Find a palettplan that matches the given package dimensions.
    
    Args:
        package_length (float): Length of the package
        package_width (float): Width of the package
        package_height (float): Height of the package
        db_path (str): Path to the database
        
    Returns:
        Optional[List[str]]: List of file names or None if not found
    """
    # if all dimensions are 0, return None
    if package_length == 0 and package_width == 0 and package_height == 0:
        return None
    
    try:
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        
        # Build query dynamically based on which dimensions are non-zero
        query = "SELECT metadata_id FROM paket_dim WHERE 1=1"
        params = []
        
        if package_length != 0:
            query += " AND length = ?"
            params.append(package_length)
            
        if package_width != 0:
            query += " AND width = ?"
            params.append(package_width)
            
        if package_height != 0:
            query += " AND height = ?"
            params.append(package_height)
            
        cursor.execute(query, params)
        results = cursor.fetchall() # get all results as a list of tuples
        
        metadata_ids = []
        for result in results:
            id = result[0]  # Get first element of the tuple
            metadata_ids.append(id)

        logger.info(f"Found {len(metadata_ids)} palettplans in database")

        # get name of file from metadata_id
        file_names = []
        for metadata_id in metadata_ids:
            cursor.execute('''
            SELECT file_name FROM paletten_metadata WHERE id = ?
            ''', (metadata_id,))
            file_name = cursor.fetchone()
            file_names.append(file_name[0].replace('.rob', ''))
        conn.close()
        return file_names
        
    except Exception as e:
        logger.error(f"Error finding palettplan in database: {e}")
        return None

def update_box_dimensions(file_name: str, height: Optional[int] = None, weight: Optional[float] = None, 
                          einzelpaket_laengs: Optional[bool] = None, db_path: str = "paletten.db", 
                          db_manager=None) -> bool:
    """Update box height, weight, and/or einzelpaket_laengs setting in the database for a specific file.
    
    This function should be called immediately when values change in the UI.
    
    Args:
        file_name (str): Name of the .rob file to update
        height (Optional[int]): New box height in mm, or None to keep current value
        weight (Optional[float]): New box weight in kg, or None to keep current value
        einzelpaket_laengs (Optional[bool]): Single package lengthwise setting, or None to keep current value
        db_path (str): Path to the database (deprecated if db_manager provided)
        db_manager: Optional HybridDatabaseManager instance
        
    Returns:
        bool: True if update was successful, False otherwise
    """
    if not file_name:
        logger.warning("Cannot update box dimensions: no file name provided")
        return False
    
    # Ensure file_name ends with .rob
    if not file_name.endswith('.rob'):
        file_name = file_name + '.rob'
    
    # Use db_manager if provided
    if db_manager:
        local_db_path = db_manager.local_db_path
    else:
        local_db_path = db_path
    
    try:
        conn = sqlite3.connect(local_db_path)
        cursor = conn.cursor()
        
        # Enable foreign key support
        cursor.execute("PRAGMA foreign_keys = ON")
        
        # Find metadata_id for this file
        cursor.execute('''
        SELECT id FROM paletten_metadata 
        WHERE file_name = ? OR file_name LIKE ?
        ''', (file_name, f"%{file_name}%"))
        result = cursor.fetchone()
        
        if not result:
            logger.warning(f"File '{file_name}' not found in database, cannot update box dimensions")
            conn.close()
            return False
        
        metadata_id = result[0]
        
        # Build update query based on which values are provided
        update_parts = []
        params = []
        
        if height is not None:
            update_parts.append("height = ?")
            params.append(height)
            
        if weight is not None:
            update_parts.append("weight = ?")
            params.append(weight)
        
        if einzelpaket_laengs is not None:
            update_parts.append("einzelpaket_laengs = ?")
            params.append(1 if einzelpaket_laengs else 0)
        
        if not update_parts:
            logger.debug("No values to update for box dimensions")
            conn.close()
            return True
        
        params.append(metadata_id)
        query = f"UPDATE paket_dim SET {', '.join(update_parts)} WHERE metadata_id = ?"
        
        cursor.execute(query, params)
        
        # Mark metadata as modified for sync
        if db_manager:
            cursor.execute('''
                UPDATE paletten_metadata 
                SET sync_status = 'modified', last_modified = ?
                WHERE id = ?
            ''', (time.time(), metadata_id))
        
        conn.commit()
        conn.close()
        
        logger.info(f"Updated box dimensions for '{file_name}': height={height}, weight={weight}, einzelpaket_laengs={einzelpaket_laengs}")
        
        # Try to sync immediately if online
        if db_manager and db_manager.is_online():
            try:
                db_manager.sync_now()
            except Exception as e:
                logger.warning(f"Failed to sync after box dimension update: {e}")
        
        return True
        
    except Exception as e:
        logger.error(f"Error updating box dimensions: {e}")
        return False


def get_box_weight(file_name: str, db_path: str = "paletten.db") -> Optional[float]:
    """Get the box weight for a specific file from the database.
    
    Args:
        file_name (str): Name of the .rob file
        db_path (str): Path to the database
        
    Returns:
        Optional[float]: Box weight in kg, or None if not found
    """
    if not file_name:
        return None
    
    # Ensure file_name ends with .rob
    if not file_name.endswith('.rob'):
        file_name = file_name + '.rob'
    
    try:
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
        SELECT pd.weight FROM paket_dim pd
        JOIN paletten_metadata pm ON pd.metadata_id = pm.id
        WHERE pm.file_name = ? OR pm.file_name LIKE ?
        ''', (file_name, f"%{file_name}%"))
        result = cursor.fetchone()
        
        conn.close()
        
        if result and result[0] is not None:
            return float(result[0])
        return None
        
    except Exception as e:
        logger.error(f"Error getting box weight: {e}")
        return None


def get_box_height(file_name: str, db_path: str = "paletten.db") -> Optional[int]:
    """Get the box height for a specific file from the database.
    
    Args:
        file_name (str): Name of the .rob file
        db_path (str): Path to the database
        
    Returns:
        Optional[int]: Box height in mm, or None if not found
    """
    if not file_name:
        return None
    
    # Ensure file_name ends with .rob
    if not file_name.endswith('.rob'):
        file_name = file_name + '.rob'
    
    try:
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
        SELECT pd.height FROM paket_dim pd
        JOIN paletten_metadata pm ON pd.metadata_id = pm.id
        WHERE pm.file_name = ? OR pm.file_name LIKE ?
        ''', (file_name, f"%{file_name}%"))
        result = cursor.fetchone()
        
        conn.close()
        
        if result and result[0] is not None:
            return int(result[0])
        return None
        
    except Exception as e:
        logger.error(f"Error getting box height: {e}")
        return None


def get_einzelpaket_laengs(file_name: str, db_path: str = "paletten.db") -> Optional[bool]:
    """Get the einzelpaket_laengs (single package lengthwise) setting for a specific file.
    
    Args:
        file_name (str): Name of the .rob file
        db_path (str): Path to the database
        
    Returns:
        Optional[bool]: True if checked, False if unchecked, None if not saved yet
    """
    if not file_name:
        return None
    
    # Ensure file_name ends with .rob
    if not file_name.endswith('.rob'):
        file_name = file_name + '.rob'
    
    try:
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
        SELECT pd.einzelpaket_laengs FROM paket_dim pd
        JOIN paletten_metadata pm ON pd.metadata_id = pm.id
        WHERE pm.file_name = ? OR pm.file_name LIKE ?
        ''', (file_name, f"%{file_name}%"))
        result = cursor.fetchone()
        
        conn.close()
        
        if result and result[0] is not None:
            return bool(result[0])
        return None
        
    except Exception as e:
        logger.error(f"Error getting einzelpaket_laengs: {e}")
        return None