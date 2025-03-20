# Dockerfile for building a Linux ARM64 executable

# Stage 1: Build the executable
FROM arm64v8/python:3.12 AS builder

# Install required dependencies
RUN apt-get update -y && apt-get install -y \
    build-essential \
    libffi-dev \
    libssl-dev \
    wget \
    libdbus-1-3 \
    libxkbcommon0 \
    libwayland-client0 \
    libwayland-cursor0 \
    libwayland-egl1 \
    libxcb-keysyms1 \
    libxcb-shape0 \
    libxcb-xfixes0 \
    libxcb-xkb1 \
    libxcb-sync1 \
    libxcb-randr0 \
    libxcb-render-util0 \
    libxcb-cursor0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-glx0 \
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