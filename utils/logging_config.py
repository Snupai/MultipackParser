import logging
from logging.handlers import RotatingFileHandler
import os
import sys
from datetime import datetime

def get_log_path() -> str:
    """Get the appropriate log file path whether running as script or frozen exe"""
    if getattr(sys, 'frozen', False):
        # Running as compiled executable
        base_path = os.path.dirname(sys.executable)
    else:
        # Running as script
        base_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    
    # Create logs directory if it doesn't exist
    logs_dir = os.path.join(base_path, 'logs')
    os.makedirs(logs_dir, exist_ok=True)
    
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_file = os.path.join(logs_dir, f'multipack_parser_{timestamp}.log')
    
    # Ensure the file can be created
    try:
        with open(log_file, 'a') as f:
            pass
    except Exception as e:
        print(f"Error creating log file: {e}")
        # Fallback to a location we know we can write to
        log_file = os.path.join(os.path.expanduser('~'), f'multipack_parser_{timestamp}.log')
    
    return log_file

def setup_logger(verbose=False) -> logging.Logger:
    """Setup and configure the logger
    
    Args:
        verbose (bool): Whether to enable verbose (DEBUG) logging
        
    Returns:
        logging.Logger: Configured logger instance
    """
    logger = logging.getLogger('multipack_parser')
    
    # Set appropriate log level based on verbose flag
    log_level = logging.DEBUG if verbose else logging.INFO
    logger.setLevel(log_level)
    
    # Remove any existing handlers
    logger.handlers.clear()
    
    try:
        log_formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
        
        # File handler
        log_path = get_log_path()
        file_handler = RotatingFileHandler(
            log_path,
            maxBytes=5*1024*1024,
            backupCount=5,
            encoding='utf-8'
        )
        file_handler.setFormatter(log_formatter)
        file_handler.setLevel(log_level)  # Use the same level for file handler
        logger.addHandler(file_handler)
        
        # Console handler
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setFormatter(log_formatter)
        console_handler.setLevel(log_level)  # Use the same level for console handler
        logger.addHandler(console_handler)
        
        logger.info(f"Logging initialized. Log file: {log_path}")
        if verbose:
            logger.debug("Verbose logging enabled")
        
    except Exception as e:
        print(f"Error setting up logger: {e}")
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s - %(message)s'))
        logger.addHandler(console_handler)
        logger.error(f"Failed to initialize file logging: {e}")
    
    return logger

# Initialize the logger with default settings (not verbose)
# This will be updated later if verbose flag is set via command line
logger = setup_logger() 