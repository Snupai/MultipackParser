# ARM64 Cross-Compilation Toolchain
# For building on x86_64 targeting Raspberry Pi (aarch64)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Cross compiler
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Sysroot (set to your ARM64 sysroot path)
# set(CMAKE_SYSROOT /path/to/aarch64-sysroot)

# Search paths
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Raspberry Pi 4 optimization flags
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a72 -mtune=cortex-a72")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a72 -mtune=cortex-a72")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-rpath-link,${CMAKE_SYSROOT}/lib/aarch64-linux-gnu")
