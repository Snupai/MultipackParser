from datetime import datetime
from enum import Enum
from typing import Optional

class MessageType(Enum):
    INFO = "info"
    WARNING = "warning" 
    ERROR = "error"
    
class Message:
    def __init__(self, text: str, type: MessageType, timestamp: Optional[datetime] = None):
        self.text = text
        self.type = type
        self.timestamp = timestamp or datetime.now()
        self.acknowledged = False
        
    def __str__(self):
        return f"[{self.timestamp.strftime('%Y-%m-%d %H:%M:%S')}] {self.type.name}: {self.text}"