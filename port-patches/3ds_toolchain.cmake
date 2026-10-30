# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 xPsycho999 (original Freeciv 3DS port)
# Copyright (C) 2026 the freeciv-3ds authors
set(CMAKE_SYSTEM_NAME Generic)

# Set devkitARM paths
set(DEVKITPRO "/opt/devkitpro")
set(DEVKITARM "${DEVKITPRO}/devkitARM")

# Set compilers
set(CMAKE_C_COMPILER "${DEVKITARM}/bin/arm-none-eabi-gcc")
set(CMAKE_CXX_COMPILER "${DEVKITARM}/bin/arm-none-eabi-g++")
set(CMAKE_AR "${DEVKITARM}/bin/arm-none-eabi-gcc-ar" CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB "${DEVKITARM}/bin/arm-none-eabi-gcc-ranlib" CACHE FILEPATH "Ranlib")

# Set compiler flags for ARM11 (3DS)
set(ARCH_FLAGS "-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ARCH_FLAGS} -O3 -mword-relocations -fomit-frame-pointer -ffast-math" CACHE STRING "C Flags" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "C++ Flags" FORCE)

# Find paths
set(CMAKE_FIND_ROOT_PATH ${DEVKITARM})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
