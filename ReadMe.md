<div align="center">
  <a href="https://github.com/Snupai/MultipackParser/actions/workflows/build.yml">
    <img alt="Build Binary" src="https://github.com/Snupai/MultipackParser/actions/workflows/build.yml/badge.svg?branch=dev-more-class-files" />
  </a>
  <a href="https://github.com/Snupai/MultipackParser/blob/main/LICENSE">
    <img src="https://img.shields.io/badge/License-GPL--3.0-blue.svg">
  </a>
</div>

# MultipackParser

MultipackParser is a tool designed to efficiently parse and manage multi-pack file structures, targeting use cases commonly encountered with the Raspberry Pi environment.

## Features

- Efficient multi-pack file parsing.
- Lightweight and performant.
- Compatible with standard Raspberry Pi distributions.

## Contributing

To edit the code, clone the repository and open the folder in your preferred IDE. The code is written in Python and uses the PySide6 framework for the GUI. To run the application, you will need to install the required dependencies using pip. You can do this by running the following command in the root directory of the project:

```bash
pip install -r requirements.txt
```

After installing the dependencies, you can run the application by executing the following command:

```bash
python main.py
```

To open the designer please use the following command:

```bash
pyside6-designer
```

Here you can open the .ui files and edit the UI elements.

To convert the .ui files to .py files, you can use the following command:

```bash
pyside6-uic MainWindow.ui -o ui_main_window.py
```

This will generate the ui_main_window.py file in the current directory.

> [!WARNING]
> If this command is run you'll have to change the line from `import MainWindowResources_rc` to `from . import MainWindowResources_rc`!

to convert the .qrc files to .py files, you can use the following command:

```bash
pyside6-rcc MainWindowResources.qrc -o MainWindowResources_rc.py
```

This will generate the MainWindowResources_rc.py file in the current directory.

Finally, to build the application, you can use the following command:

> [!WARNING]
> This command will need Docker to be installed on your system.
> It will build the application using the Dockerfile in the root directory of the project.
> The resulting binary will be placed in the local_dist directory.
> The binary is only compatible with Arm64 architecture.

```bash
python build.py
```

## License

This project is licensed under GNU General Public License v3.0.
