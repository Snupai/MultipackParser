from datetime import datetime
from enum import Enum
from typing import Optional

class MessageType(Enum):
    """Enum for the message types.

    Args:
        Enum (Enum): The parent class of the message type
    """
    INFO = "info"
    WARNING = "warning" 
    ERROR = "error"
    
class Message:
    """Message class.
    """
    def __init__(self, text: str, type: MessageType, timestamp: Optional[datetime] = None):
        """Initialize the message.

        Args:
            text (str): The message text
            type (MessageType): The message type
            timestamp (Optional[datetime], optional): The timestamp of the message. Defaults to None.
        """
        self.text = text
        self.type = type
        self.timestamp = timestamp or datetime.now()
        self.acknowledged = False
        
    def __str__(self):
        """String representation of the message.

        Returns:
            str: The string representation of the message
        """
        return f"[{self.timestamp.strftime('%Y-%m-%d %H:%M:%S')}] {self.type.name}: {self.text}"