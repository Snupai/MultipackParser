# -*- mode: python ; coding: utf-8 -*-

import os
import sys
from PyInstaller.utils.hooks import collect_data_files, collect_submodules, collect_dynamic_libs

block_cipher = None

# Collect Qt plugins using collect_data_files
qt_plugins_datas = []
for plugin in ['platforms', 'platforminputcontexts', 'virtualkeyboard']:
    plugin_datas = collect_data_files('PySide6', include_py_files=False, 
                                    subdir=os.path.join('Qt6', 'plugins', plugin))
    qt_plugins_datas.extend(plugin_datas)

# Minimal set of required Qt modules
qt_modules = [
    'PySide6.QtCore',
    'PySide6.QtGui',
    'PySide6.QtWidgets',
    'PySide6.QtVirtualKeyboard'
]

a = Analysis(
    ['main.py'],
    pathex=[],
    binaries=None,
    datas=qt_plugins_datas,
    hiddenimports=qt_modules,
    hookspath=['hooks'],
    hooksconfig={
        'pyside6': {
            'modules': ['QtCore', 'QtGui', 'QtWidgets', 'QtVirtualKeyboard'],
            'plugins': ['platforms', 'platforminputcontexts', 'virtualkeyboard']
        }
    },
    runtime_hooks=['hooks/runtime_hook.py'],
    excludes=[
        'tkinter', 'matplotlib.tests', 'numpy.random._examples',
        'PIL', 'PySide6.QtNetwork', 'PySide6.QtPrintSupport',
        'PySide6.QtSensors', 'PySide6.QtSerialPort', 'PySide6.QtSql',
        'PySide6.QtTest', 'PySide6.QtWebChannel', 'PySide6.QtWebEngine',
        'PySide6.QtWebEngineCore', 'PySide6.QtWebEngineWidgets',
        'PySide6.QtWebKit', 'PySide6.QtWebKitWidgets', 'PySide6.QtWebSockets',
        'PySide6.QtXml', 'PySide6.QtXmlPatterns', 'PySide6.QtMultimedia',
        'PySide6.QtOpenGL', 'PySide6.QtPositioning', 'PySide6.QtQuickWidgets'
    ],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

# Create the PYZ object with compression
pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

# Create the executable with optimized settings
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
    strip=False,  # Enabling this breaks the binaries functionality and only saves like 10MB which in the scale of 200MB is nothing
    upx=True,
    upx_exclude=['vcruntime140.dll'],  # Exclude problematic files from UPX compression
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=None,
) 