from typing import List, Optional, Union, Set
from datetime import datetime
from .message import Message, MessageType
from ..logging_config import logger

class MessageManager:
    """Manager for messages.
    """
    def __init__(self):
        """Initialize the message manager.
        """
        logger.debug("Initializing MessageManager")
        self._messages: List[Message] = []
        self._active_messages: List[Message] = []
        self._blocked_messages: Set[str] = set()
        
    def add_message(self, text: str, type: MessageType, block: bool = False) -> Message:
        """Add a new message.

        Args:
            text (str): The message text
            type (MessageType): The message type
            block (bool, optional): Whether to block the message from being acknowledged. Defaults to False.

        Returns:
            Message: The new message
        """
        logger.debug(f"Adding message: {text} (type: {type}, block: {block})")
        message = Message(text, type)
        self._messages.append(message)
        self._active_messages.append(message)
        if block:
            logger.debug(f"Blocking message: {text}")
            self._blocked_messages.add(text)
        return message
        
    def block_message(self, text: str) -> None:
        """Block a message from being acknowledged.

        Args:
            text (str): The text of the message to block
        """
        logger.debug(f"Blocking message: {text}")
        self._blocked_messages.add(text)
        
    def unblock_message(self, text: str) -> None:
        """Unblock a message from being acknowledged.

        Args:
            text (str): The text of the message to unblock
        """
        logger.debug(f"Unblocking message: {text}")
        self._blocked_messages.discard(text)
        
    def acknowledge_message(self, message: Union[Message, str]) -> bool:
        """Acknowledge a message.

        Args:
            message (Union[Message, str]): The message to acknowledge

        Returns:
            bool: True if the message was acknowledged, False otherwise
        """
        text = message.text if isinstance(message, Message) else message
        logger.debug(f"Attempting to acknowledge message: {text}")
        
        if text in self._blocked_messages:
            logger.debug(f"Message is blocked, cannot acknowledge: {text}")
            return False
            
        if isinstance(message, str):
            msg = self.get_active_message(text)
            if msg is None:
                logger.warning(f"Message not found: {text}")
                return False
            message = msg
            
        if message in self._active_messages:
            logger.info(f"Message acknowledged: {text}")
            message.acknowledged = True
            self._active_messages.remove(message)
            return True
            
        logger.warning(f"Message not active, cannot acknowledge: {text}")
        return False

    def get_all_messages(self) -> List[Message]:
        """Get all messages.

        Returns:
            List[Message]: A list of all messages
        """
        return self._messages.copy()
        
    def get_active_messages(self) -> List[Message]:
        """Get all active messages.

        Returns:
            List[Message]: A list of all active messages
        """
        return self._active_messages.copy()
        
    def get_latest_message(self) -> Optional[Message]:
        """Get the latest message.

        Returns:
            Optional[Message]: The latest message
        """
        return self._active_messages[-1] if self._active_messages else None

    def get_active_message(self, text: str) -> Optional[Message]:
        """Get the active message with the given text.

        Args:
            text (str): The text of the message to get

        Returns:
            Optional[Message]: The active message with the given text
        """
        for message in self._active_messages:
            if message.text == text:
                return message
        return None
