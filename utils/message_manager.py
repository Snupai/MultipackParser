from typing import List, Optional
from datetime import datetime
from .message import Message, MessageType

class MessageManager:
    def __init__(self):
        self._messages: List[Message] = []
        self._active_messages: List[Message] = []
        
    def add_message(self, text: str, type: MessageType) -> Message:
        message = Message(text, type)
        self._messages.append(message)
        self._active_messages.append(message)
        return message
        
    def acknowledge_message(self, message: Message) -> None:
        if message in self._active_messages:
            message.acknowledged = True
            self._active_messages.remove(message)
            
    def get_all_messages(self) -> List[Message]:
        return self._messages.copy()
        
    def get_active_messages(self) -> List[Message]:
        return self._active_messages.copy()
        
    def get_latest_message(self) -> Optional[Message]:
        return self._active_messages[-1] if self._active_messages else None