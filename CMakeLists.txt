cmake_minimum_required(VERSION 3.16)
project(cube_mod CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Emit build/compile_commands.json so clangd (the VSCodium C/C++ IntelliSense
# engine) knows the include paths, defines, and mingw cross-compile target.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type: -DCMAKE_BUILD_TYPE, else $ENV{CUBE_MOD_BUILD_TYPE}, else Release.
if(NOT CMAKE_BUILD_TYPE)
  if(DEFINED ENV{CUBE_MOD_BUILD_TYPE})
    set(CMAKE_BUILD_TYPE "$ENV{CUBE_MOD_BUILD_TYPE}")
  else()
    set(CMAKE_BUILD_TYPE Release)
  endif()
endif()

# Both components emit their artifacts into the top-level build dir.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
# Visual Studio (a multi-config generator) otherwise nests artifacts in a per-config
# subdir; pin every config to the same dir so build.sh / build.bat find them uniformly.
foreach(cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg} "${CMAKE_BINARY_DIR}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg} "${CMAKE_BINARY_DIR}")
endforeach()

# Compiler-family flags shared by both components. Visual Studio / MSVC and the mingw
# cross-toolchain take different warning, runtime, and link flags; each component reads
# these variables so the per-target rules stay identical across toolchains.
if(MSVC)
  add_compile_options(/MP)
  # Static CRT so the injected DLL carries no vcredist dependency (mirrors mingw -static).
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  # /W3 with 4100 (unreferenced formal parameter) silenced: detour signatures carry
  # ABI-mandated but unused params, matching the mingw -Wno-unused-parameter.
  set(CUBE_WARNING_FLAGS /W3 /wd4100)
  set(CUBE_NO_WARNING_FLAGS /w)
else()
  set(CUBE_WARNING_FLAGS -Wall -Wextra -Wno-unused-parameter)
  set(CUBE_NO_WARNING_FLAGS -w)
endif()

# modloader builds both cube_mod.dll and the inject.exe injector (part of it).
add_subdirectory(modloader)
# example_lib is a headless service provider; example_mod depends on it (and includes its contract header).
add_subdirectory(example_lib)
add_subdirectory(example_mod)
