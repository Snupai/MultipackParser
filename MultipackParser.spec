# -*- mode: python ; coding: utf-8 -*-

import os
import sys
from PyInstaller.utils.hooks import collect_data_files, collect_submodules, collect_all

block_cipher = None

# Collect only essential Qt plugins
qt_plugins = [
    ('PySide6/plugins/platforminputcontexts', '.'),
    ('PySide6/plugins/virtualkeyboard', '.'),
]

# Collect only essential Qt virtual keyboard modules
qt_virtual_keyboard_binaries = []
qt_virtual_keyboard_datas = []
qt_virtual_keyboard_hiddenimports = []

# Check if we're on Linux ARM platform
is_arm_linux = False
if sys.platform.startswith('linux'):
    try:
        is_arm_linux = 'arm' in os.uname().machine
    except:
        import platform
        is_arm_linux = 'arm' in platform.machine().lower()

# Collect only essential QtVirtualKeyboard components
vkb_imports, vkb_datas, vkb_binaries = collect_all('PySide6.QtVirtualKeyboard')
qt_virtual_keyboard_hiddenimports.extend(vkb_imports)
qt_virtual_keyboard_datas.extend(vkb_datas)
qt_virtual_keyboard_binaries.extend(vkb_binaries)

# Create the analysis object with optimized settings
a = Analysis(
    ['main.py'],
    pathex=[],
    binaries=qt_virtual_keyboard_binaries,
    datas=qt_virtual_keyboard_datas,
    hiddenimports=qt_virtual_keyboard_hiddenimports,
    hookspath=['hooks'],
    hooksconfig={},
    runtime_hooks=['hooks/runtime_hook.py'],
    excludes=[
        'tkinter', 'matplotlib.tests', 'numpy.random._examples',
        'PIL.ImageQt', 'PySide6.QtNetwork', 'PySide6.QtPrintSupport',
        'PySide6.QtSensors', 'PySide6.QtSerialPort', 'PySide6.QtSql',
        'PySide6.QtTest', 'PySide6.QtWebChannel', 'PySide6.QtWebEngine',
        'PySide6.QtWebEngineCore', 'PySide6.QtWebEngineWidgets',
        'PySide6.QtWebKit', 'PySide6.QtWebKitWidgets', 'PySide6.QtWebSockets',
        'PySide6.QtXml', 'PySide6.QtXmlPatterns'
    ],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

# Create the PYZ object
pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

# Create the executable with optimized UPX settings
exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='MultipackParser',
    debug=False,
    bootloader_ignore_signals=False,
    strip=True,  # Enable stripping to reduce binary size; idk if this breaks the functionality.
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=None,
) 