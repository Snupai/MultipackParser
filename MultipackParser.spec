# -*- mode: python ; coding: utf-8 -*-

import os
import sys
from PyInstaller.utils.hooks import collect_data_files, collect_submodules, collect_all

block_cipher = None

# Collect Qt plugins, especially for virtual keyboard
qt_plugins = [
    ('PySide6/plugins/platforminputcontexts', '.'),
    ('PySide6/plugins/virtualkeyboard', '.'),
]

# Collect all Qt virtual keyboard modules and data
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

# Collect all QtVirtualKeyboard related files and modules
vkb_imports, vkb_datas, vkb_binaries = collect_all('PySide6.QtVirtualKeyboard')
qt_virtual_keyboard_hiddenimports.extend(vkb_imports)
qt_virtual_keyboard_datas.extend(vkb_datas)
qt_virtual_keyboard_binaries.extend(vkb_binaries)

# Create the analysis object with all our collected data
a = Analysis(
    ['main.py'],
    pathex=[],
    binaries=qt_virtual_keyboard_binaries,
    datas=qt_virtual_keyboard_datas,
    hiddenimports=['PySide6.QtVirtualKeyboard'] + qt_virtual_keyboard_hiddenimports,
    hookspath=['hooks'],
    hooksconfig={},
    runtime_hooks=['hooks/runtime_hook.py'],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

# Create the PYZ object
pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

# Create the executable
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
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon='path_to_your_icon.ico',
) 