# Cross-compiles Kinetra-Lab for 64-bit Windows with the MinGW-w64 GCC toolchain
# (Arch Linux: `pacman -S mingw-w64-gcc`). Used by the `windows` presets; see
# docs/windows.md.
#
# The executables run on Windows, and from WSL directly through WSL interop, so
# CTest and doctest test discovery work without an emulator there.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(mingw_triple x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${mingw_triple}-gcc)
set(CMAKE_CXX_COMPILER ${mingw_triple}-g++)
set(CMAKE_RC_COMPILER ${mingw_triple}-windres)

# Search Windows libraries and packages only in the MinGW sysroot, never the host's.
set(CMAKE_FIND_ROOT_PATH /usr/${mingw_triple})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Link the GCC runtime (libstdc++, libgcc, winpthread) statically, so an executable
# needs only SDL2.dll beside it. Imported DLLs such as SDL2 stay dynamic.
set(CMAKE_EXE_LINKER_FLAGS_INIT -static)
