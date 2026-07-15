# Cross-compile 32-bit Windows binaries from Linux (Cube.exe is a 32-bit PE).
# Override the cross-compiler with $ENV{MINGW_PREFIX} if it differs from the default.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

if(DEFINED ENV{MINGW_PREFIX})
  set(TOOLCHAIN_PREFIX "$ENV{MINGW_PREFIX}")
else()
  set(TOOLCHAIN_PREFIX i686-w64-mingw32)
endif()

set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

# Sysroot: Arch/Debian keep it at /usr/<prefix>; override with $ENV{MINGW_SYSROOT}
# on distros that differ (Fedora: /usr/<prefix>/sys-root/mingw; Homebrew: another prefix).
if(DEFINED ENV{MINGW_SYSROOT})
  set(CMAKE_FIND_ROOT_PATH "$ENV{MINGW_SYSROOT}")
else()
  set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
endif()
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
