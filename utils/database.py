import sqlite3
import os
import time
import datetime
import logging
from typing import Union, List, Dict, Any, Optional, Tuple, Literal

from utils import global_vars

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
        FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
    )
    ''')
    
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
        
        # Read and parse the file line by line
        with open(file_path) as file:
            for line in file:
                # Convert each line to list of integers
                tmpList = [int(x) for x in line.strip().split('\t')]
                g_Daten.append(tmpList)
 
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
        print(f"Error reading file {filename}: {e}")
    return 1, None, None

def save_to_database(file_name, db_path="paletten.db") -> bool:
    """Save all global data to the database.
    
    Args:
        db_path (str): Path to the database
        file_path (str): Path to the source .rob file that was parsed
        file_timestamp (float): Timestamp of when the source file was last modified
        
    Returns:
        bool: True if data was saved, False if skipped due to older timestamp
    """
    # Create database tables if they don't exist
    create_database(db_path)
    
    _, file_timestamp, g_Daten, g_LageZuordnung, g_PaketPos, g_PaketeZuordnung, g_Zwischenlagen, g_paket_quer, g_CenterOfGravity, g_PalettenDim, g_PaketDim, g_LageArten, g_AnzLagen, g_AnzahlPakete = UR_ReadDataFromUsbStick(file_name, global_vars.PATH_USB_STICK)
    
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
        INSERT INTO paket_dim (metadata_id, length, width, height, gap) 
        VALUES (?, ?, ?, ?, ?)
        ''', (metadata_id, g_PaketDim[0], g_PaketDim[1], 
              g_PaketDim[2], g_PaketDim[3]))
    
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
    
    conn.commit()
    conn.close()
    return True

def load_from_database(db_path="paletten.db", file_name=None, metadata_id=None) -> Union[Literal[0], Literal[1]]:
    """Load all data from the database to global variables.
    
    Args:
        db_path (str): Path to the database
        file_name (str, optional): Specific .rob filename to load. If None, loads the most recent entry.
        metadata_id (int, optional): Specific metadata ID to load. Takes precedence over file_name.
        
    Returns:
        Union[Literal[0], Literal[1]]: 0 if successful, 1 otherwise.
    """
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
        
        # Load package dimensions [length,width,height,gap]
        cursor.execute('''
        SELECT length, width, height, gap FROM paket_dim 
        WHERE metadata_id = ?
        ''', (metadata_id,))
        result = cursor.fetchone()
        if result:
            g_PaketDim = list(result)
        
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
        
        return g_Daten, g_LageZuordnung, g_PaketPos, g_PaketeZuordnung, g_Zwischenlagen, g_paket_quer, g_CenterOfGravity, g_PalettenDim, g_PaketDim, g_LageArten, g_AnzLagen, g_AnzahlPakete
        
        return 0
    except Exception as e:
        logger.error(f"Error loading data from database: {e}")
        return 1

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
