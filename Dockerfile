# Dockerfile for MultipackParser C++ x86_64 Build
# Based on Ubuntu 22.04 with Qt6

FROM ubuntu:22.04

LABEL maintainer="Szaidel Cosmetic GmbH"
LABEL description="Build environment for MultipackParser C++ (x86_64)"

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Berlin

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    git \
    libgl1-mesa-dev \
    libegl1-mesa-dev \
    libgles2-mesa-dev \
    qt6-base-dev \
    qt6-base-dev-tools \
    qt6-tools-dev \
    qt6-tools-dev-tools \
    qt6-multimedia-dev \
    libqt6sql6-sqlite \
    libsqlite3-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Set up environment
ENV PATH="/usr/lib/qt6/bin:${PATH}"
ENV Qt6_DIR="/usr/lib/x86_64-linux-gnu/cmake/Qt6"

# Create working directory
WORKDIR /app

# Copy source files
COPY CMakeLists.txt ./
COPY cmake/ ./cmake/
COPY include/ ./include/
COPY src/ ./src/
COPY ui/ ./ui/
COPY resources/ ./resources/
COPY tests/ ./tests/

# Configure
RUN cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DENABLE_VTK=OFF

ARG CMAKE_BUILD_JOBS=4

# Build
RUN cmake --build build --target multipack-parser -j${CMAKE_BUILD_JOBS}

# Verify the binary was built
RUN ls -la build/bin/
