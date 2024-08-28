# syntax=docker/dockerfile:1

# Use an official Python runtime as a parent image
FROM arm64v8/python:3.11

# Set the working directory in the container
WORKDIR /app

# Copy the current directory contents into the container at /app
COPY . /app

# Install necessary system dependencies for running a PySide6 application
RUN apt-get update && apt-get install -y \
    libgl1-mesa-glx \
    libxkbcommon-x11-0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-randr0 \
    libxcb-render-util0 \
    libxcb-render0 \
    libxcb-shape0 \
    libxcb-sync1 \
    libxcb-xfixes0 \
    libxcb-xinerama0 \
    libxcb-xkb1 \
    libxcb1 \
    libxrender1 \
    libxi6 \
    libdbus-1-3 \
    libxcb-cursor0 \
    libegl1 \
    ccache \
    ffmpeg \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install necessary packages for virtual keyboard modules
RUN apt-get update && apt-get install -y \
    qml-module-qtquick-virtualkeyboard \
    qml-module-qtquick-layouts \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install PySide6 and pyside6-deploy
RUN pip install PySide6 pydub Nuitka==2.3.2 ordered-set zstandard

# Set environment variable for Qt Virtual Keyboard
ENV QT_IM_MODULE=qtvirtualkeyboard

# Run pyside6-deploy to create the executable with Qt Virtual Keyboard
RUN yes | pyside6-deploy -c pysidedeploy.spec --extra-modules=QtVirtualKeyboard

# Command to run the application
CMD ["./main.bin"]