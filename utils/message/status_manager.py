import logging
from typing import Optional
from .. import global_vars
from .message import MessageType, Message
from .message_manager import MessageManager

logger = logging.getLogger(__name__)

def update_status_label(text: str, color: str, blink: bool = False, second_color: Optional[str] = None, instant_acknowledge: bool = False, block: bool = False) -> None:
    """Update the status label with the given text and color.

    Args:
        text (str): The text to be displayed in the status label.
        color (str): The color of the status label.
        blink (bool, optional): Whether the status label should blink. Defaults to False.
        second_color (Optional[str], optional): The second color of the status label. Defaults to None.
        instant_acknowledge (bool, optional): Whether the status label should be acknowledged immediately. Defaults to False.
        block (bool, optional): Whether the status label should be blocked. Defaults to False.
    """
    if not global_vars.ui:
        logger.error("UI not initialized")
        return

    # Add message to manager
    if global_vars.message_manager is None:
        global_vars.message_manager = MessageManager()
        
    message_type = {
        "red": MessageType.ERROR,
        "orange": MessageType.WARNING,
        "green": MessageType.INFO,
        "black": MessageType.INFO
    }.get(color, MessageType.INFO)
    
    message: Message = global_vars.message_manager.add_message(text, message_type, block=block)
    if instant_acknowledge:
        global_vars.message_manager.acknowledge_message(message)

    if not hasattr(global_vars.ui, 'LabelPalletenplanInfo'):
        logger.error("Label not found in UI")
        return

    # Import here to avoid circular dependency
    from ui_files.BlinkingLabel import BlinkingLabel

    # Create new blinking label only if it doesn't exist
    if global_vars.blinking_label is None:
        global_vars.blinking_label = BlinkingLabel(
            text, 
            color, 
            global_vars.ui.LabelPalletenplanInfo.geometry(), 
            parent=global_vars.ui.stackedWidget.widget(0),
            second_color=second_color,
            font=global_vars.ui.LabelPalletenplanInfo.font(),
            alignment=global_vars.ui.LabelPalletenplanInfo.alignment()
        )
        global_vars.ui.LabelPalletenplanInfo.hide()
    else:
        global_vars.blinking_label.update_text(text)
        global_vars.blinking_label.update_color(color, second_color)

    # Update blinking state
    if blink:
        global_vars.blinking_label.start_blinking()
    else:
        global_vars.blinking_label.stop_blinking()