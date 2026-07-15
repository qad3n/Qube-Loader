# The mod loader DLL (cube_mod.dll): hooks the game, loads mods, serves the API.
# No ImGui / UI here; that lives in mods (see example_mod). Sources auto-discovered.
file(GLOB_RECURSE MOD_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
if(NOT MOD_SOURCES)
  message(FATAL_ERROR "no sources found under modloader/src/")
endif()

# MinHook: the inline-hook engine backing every detour (D3D9 EndScene/Reset, the
# selection/pickup detours, and the mod-facing game-function hooks in game/gamehooks).
# Vendored as a git submodule under include/minhook (the loader's own
# vendored-dependency dir, distinct from a mod's include/). Sources are referenced
# explicitly rather than globbed, because a full upstream checkout ships tooling we
# do not build. Not exposed to mods.
set(MINHOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/minhook")
if(NOT EXISTS "${MINHOOK_DIR}/include/MinHook.h")
  message(FATAL_ERROR "MinHook submodule missing at modloader/include/minhook. "
    "Run: git submodule update --init --recursive")
endif()
set(MINHOOK_SOURCES
  "${MINHOOK_DIR}/src/buffer.c"
  "${MINHOOK_DIR}/src/hook.c"
  "${MINHOOK_DIR}/src/trampoline.c"
  "${MINHOOK_DIR}/src/hde/hde32.c"
  "${MINHOOK_DIR}/src/hde/hde64.c")

add_library(cube_mod SHARED ${MOD_SOURCES} ${MINHOOK_SOURCES})
# Enforce a warning-clean loader. -Wno-unused-parameter: detour signatures carry ABI-mandated but
# unused params (the __thiscall dummy edx). MinHook's own C sources are silenced below.
target_compile_options(cube_mod PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CUBE_WARNING_FLAGS}>)
set_source_files_properties(${MINHOOK_SOURCES} PROPERTIES COMPILE_OPTIONS "${CUBE_NO_WARNING_FLAGS}")
target_include_directories(cube_mod PRIVATE src "${CMAKE_CURRENT_SOURCE_DIR}/sdk")
# MinHook's public header as SYSTEM so "MinHook.h" resolves and its warnings stay
# out of our build.
target_include_directories(cube_mod SYSTEM PRIVATE "${MINHOOK_DIR}/include")
set_target_properties(cube_mod PROPERTIES PREFIX "" OUTPUT_NAME "cube_mod")
# psapi: module enumeration. d3d9: hidden probe device to grab the EndScene vtable.
# dinput8 + dxguid: hidden probe device to grab the IDirectInputDevice8 vtable (hooks::dinput
# unacquires the game's DI mouse/keyboard while an overlay is open so window input reaches it).
# input_block freezes game movement/camera via a user32 GetFocus IAT hook.
target_link_libraries(cube_mod PRIVATE psapi d3d9 dinput8 dxguid)
# mingw: fold libgcc/libstdc++ in so the DLL is self-contained. MSVC does the same via
# the static CRT set at the top level (CMAKE_MSVC_RUNTIME_LIBRARY).
if(NOT MSVC)
  target_link_options(cube_mod PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# The injector (inject.exe) is part of the modloader: a standalone tool that
# LoadLibrary-injects cube_mod.dll into the game. Its source lives under
# modloader/injector/ (not src/, so it stays out of the DLL glob above).
file(GLOB_RECURSE INJECTOR_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/injector/*.cpp")
if(NOT INJECTOR_SOURCES)
  message(FATAL_ERROR "no sources found under modloader/injector/")
endif()
add_executable(inject ${INJECTOR_SOURCES})
target_compile_options(inject PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CUBE_WARNING_FLAGS}>)
if(NOT MSVC)
  target_link_options(inject PRIVATE -static -static-libgcc -static-libstdc++)
endif()
