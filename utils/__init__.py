# utils_folder/__init__.py

from .system.core import global_vars
from .ui import notification_popup
from .system.config.settings import Settings

__all__ = ['global_vars', 'notification_popup', 'Settings']