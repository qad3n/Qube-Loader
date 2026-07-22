# Example mod: an ImGui menu that reads player data through the CubeApi. The LOADER owns the ImGui
# context + the DX9/Win32 backends + the whole overlay lifecycle (see modloader/src/overlay/ and
# CubeOverlayApi); this mod only compiles ImGui CORE and draws into the loader's shared context via
# mod.menu(). Builds into build/mods/ so the loader finds it on first run.
file(GLOB_RECURSE EXAMPLE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(example_mod SHARED ${EXAMPLE_SOURCES})
target_include_directories(example_mod PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/modloader/sdk"
  "${CMAKE_SOURCE_DIR}/example_lib/include")

# ImGui CORE only (no backends): the loader owns the backends + context. cube_add_imgui compiles the
# same submodule the loader ships (modloader/sdk/imgui) so the shared context is layout-compatible, adds
# its headers as SYSTEM includes, and silences its warnings. This one line is all a menu mod needs -
# and because it puts imgui.h on the include path, cube_mod.hpp auto-enables mod.menu().
include("${CMAKE_SOURCE_DIR}/modloader/sdk/cube_imgui.cmake")
cube_add_imgui(example_mod)

# Warning-clean first-party sources (ImGui's own .cpp are silenced by cube_add_imgui).
target_compile_options(example_mod PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CUBE_WARNING_FLAGS}>)
# -Wno-format-truncation (GCC/mingw only): the event console writes deliberately bounded,
# truncatable display lines. MSVC has no equivalent warning to silence.
if(NOT MSVC)
  target_compile_options(example_mod PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-format-truncation>)
endif()
# Emit into build/mods/ for every config (the _<CONFIG> variants keep Visual Studio from
# nesting a per-config subdir under mods/).
set_target_properties(example_mod PROPERTIES
  PREFIX ""
  OUTPUT_NAME "example_mod"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/mods"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/mods"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/mods"
  LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/mods"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/mods"
  LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/mods"
  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/mods"
  LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/mods"
  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/mods"
  LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/mods")
# Ship this mod's locale files where the loader looks: <build>/lang/<stem>/. The loader roots locales
# at <dllDir>/lang and keys per-mod by DLL stem (example_mod), so translate() finds them at runtime.
add_custom_command(TARGET example_mod POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/lang/example_mod"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/lang" "${CMAKE_BINARY_DIR}/lang/example_mod"
  COMMENT "Copying example_mod locale files to build/lang/example_mod")

if(NOT MSVC)
  target_link_options(example_mod PRIVATE -static -static-libgcc -static-libstdc++)
endif()
