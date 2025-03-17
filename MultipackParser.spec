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

# Collect all PySide6 dependencies needed for the application
qt_data_files = []
qt_binaries = []
qt_hiddenimports = []

# Collect essential Qt modules
for module in ['PySide6.QtCore', 'PySide6.QtGui', 'PySide6.QtWidgets', 'PySide6.QtQml', 'PySide6.QtQuick']:
    imports, datas, binaries = collect_all(module)
    qt_hiddenimports.extend(imports)
    qt_data_files.extend(datas)
    qt_binaries.extend(binaries)

# Try to collect QtVirtualKeyboard related files, but don't fail if not found
try:
    vkb_imports, vkb_datas, vkb_binaries = collect_all('PySide6.QtVirtualKeyboard')
    qt_hiddenimports.extend(vkb_imports)
    qt_data_files.extend(vkb_datas)
    qt_binaries.extend(vkb_binaries)
except ImportError:
    print("INFO: PySide6.QtVirtualKeyboard not found, but we'll continue without it")
    # The virtual keyboard will be enabled via QT_IM_MODULE environment variable
    # in the runtime hook

# Check if we're on Linux ARM platform
is_arm_linux = False
if sys.platform.startswith('linux'):
    try:
        is_arm_linux = 'arm' in os.uname().machine
    except:
        import platform
        is_arm_linux = 'arm' in platform.machine().lower()

# Create the analysis object with all our collected data
a = Analysis(
    ['main.py'],
    pathex=[],
    binaries=qt_binaries,
    datas=qt_data_files,
    hiddenimports=qt_hiddenimports + [
        'matplotlib',
        'matplotlib.backends.backend_qtagg',
    ],
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