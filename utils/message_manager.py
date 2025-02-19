from typing import List, Optional, Union, Set
from datetime import datetime
from .message import Message, MessageType

class MessageManager:
    def __init__(self):
        self._messages: List[Message] = []
        self._active_messages: List[Message] = []
        self._blocked_messages: Set[str] = set()
        
    def add_message(self, text: str, type: MessageType, block: bool = False) -> Message:
        """Add a new message.
        
        Args:
            text (str): The message text
            type (MessageType): The message type
            block (bool, optional): Whether to block the message from being acknowledged. Defaults to False.
        """
        message = Message(text, type)
        self._messages.append(message)
        self._active_messages.append(message)
        if block:
            self._blocked_messages.add(text)
        return message
        
    def block_message(self, text: str) -> None:
        """Block a message from being acknowledged."""
        self._blocked_messages.add(text)
        
    def unblock_message(self, text: str) -> None:
        """Allow a message to be acknowledged again."""
        self._blocked_messages.discard(text)
        
    def acknowledge_message(self, message: Union[Message, str]) -> bool:
        text = message.text if isinstance(message, Message) else message
        if text in self._blocked_messages:
            return False
            
        if isinstance(message, str):
            msg: Message | None = self.get_active_message(message)
            if msg is None:
                return False
            message = msg
        if message in self._active_messages:
            message.acknowledged = True
            self._active_messages.remove(message)
            return True
        return False

    def get_all_messages(self) -> List[Message]:
        return self._messages.copy()
        
    def get_active_messages(self) -> List[Message]:
        return self._active_messages.copy()
        
    def get_latest_message(self) -> Optional[Message]:
        return self._active_messages[-1] if self._active_messages else None

    def get_active_message(self, text: str) -> Optional[Message]:
        """Get the active message with the given text.

        Args:
            text (str): The text of the message to get.

        Returns:
            Optional[Message]: The active message with the given text.
        """
        for message in self._active_messages:
            if message.text == text:
                return message
        return None
