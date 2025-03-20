# Dockerfile for building a Linux ARM64 executable

# Stage 1: Build the executable
FROM arm64v8/python:3.12 AS builder

# Install required dependencies
RUN apt-get update -y && apt-get install -y \
    build-essential \
    libffi-dev \
    libssl-dev \
    wget \
    # X11 and GUI dependencies
    libx11-xcb1 \
    libxcb1 \
    libxcb-glx0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-randr0 \
    libxcb-render-util0 \
    libxcb-shape0 \
    libxcb-sync1 \
    libxcb-xfixes0 \
    libxcb-xkb1 \
    # Qt dependencies
    libxkbcommon0 \
    libxkbcommon-x11-0 \
    libxkbfile1 \
    # Multimedia dependencies
    libasound2 \
    libpulse0 \
    libopus0 \
    # Image format dependencies
    libtiff5 \
    libwebp6 \
    # Additional X11 dependencies
    libx11-xcb1 \
    libxcomposite1 \
    libxdamage1 \
    libxfixes3 \
    libxtst6 \
    libxrandr2 \
    # WebEngine dependencies
    libgbm1 \
    libnspr4 \
    libnss3 \
    libnss3-tools \
    libsmime3 \
    # Wayland dependencies
    libwayland-client0 \
    libwayland-cursor0 \
    libwayland-egl1 \
    libxcb1 \
    libxkbcommon0 \
    libxkbcommon-x11-0 \
    libwayland-client0 \
    libwayland-cursor0 \
    libwayland-egl1 \
    libgtk-3-0 \
    # GTK dependencies
    libgtk-3-0 \
    libgdk-3-0 \
    # Compression dependencies
    libminizip1 \
    && apt-get clean

# Install PyInstaller and required Python packages
RUN pip install --upgrade pip
RUN pip install pyinstaller pyside6 pyinstaller-hooks-contrib tomli_w matplotlib cryptography

# Set the working directory in the container
WORKDIR /app

# Copy the entire project into the container
COPY . .

# Create hooks directory for PyInstaller
RUN mkdir -p ./hooks

# Run PyInstaller using our spec file
RUN pyinstaller MultipackParser.spec

# Stage 2: Copy the executable to a minimal base image
FROM arm64v8/python:3.12-slim

WORKDIR /app

# Copy the executable to the app directory
COPY --from=builder /app/dist/MultipackParser /app/dist/MultipackParser

# Set entrypoint to the built executable
ENTRYPOINT ["/app/dist/MultipackParser/MultipackParser"]