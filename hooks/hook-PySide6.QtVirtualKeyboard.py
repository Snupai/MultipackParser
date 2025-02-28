from PyInstaller.utils.hooks import collect_data_files, collect_submodules

# Include all data files for QtVirtualKeyboard
datas = collect_data_files('PySide6', subdir='QtVirtualKeyboard')

# Include all submodules from QtVirtualKeyboard
hiddenimports = collect_submodules('PySide6.QtVirtualKeyboard') 