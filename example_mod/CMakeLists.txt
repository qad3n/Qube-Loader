# Example mod: an ImGui menu that reads player data through the CubeApi. It owns
# its own ImGui (the loader has none), so there is no shared-context problem.
# Builds into build/mods/ so the loader finds it on first run.
file(GLOB_RECURSE EXAMPLE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# ImGui (Dear ImGui) vendored as a git submodule under include/imgui (this mod's
# own vendored-dependency dir). We compile only the core plus the DX9 + Win32
# backends we use; a full upstream checkout also ships every other backend and the
# examples/, which we must not build, so sources are listed explicitly.
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/imgui")
if(NOT EXISTS "${IMGUI_DIR}/imgui.h")
  message(FATAL_ERROR "ImGui submodule missing at example_mod/include/imgui. "
    "Run: git submodule update --init --recursive")
endif()
set(IMGUI_SOURCES
  "${IMGUI_DIR}/imgui.cpp"
  "${IMGUI_DIR}/imgui_draw.cpp"
  "${IMGUI_DIR}/imgui_tables.cpp"
  "${IMGUI_DIR}/imgui_widgets.cpp"
  "${IMGUI_DIR}/backends/imgui_impl_dx9.cpp"
  "${IMGUI_DIR}/backends/imgui_impl_win32.cpp")

add_library(example_mod SHARED ${EXAMPLE_SOURCES} ${IMGUI_SOURCES})
target_include_directories(example_mod PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/modloader/sdk"
  "${CMAKE_SOURCE_DIR}/example_lib/include")
# ImGui core + backends headers as SYSTEM so "imgui.h" / "imgui_impl_*.h" resolve
# and third-party warnings stay out of our build.
target_include_directories(example_mod SYSTEM PRIVATE "${IMGUI_DIR}" "${IMGUI_DIR}/backends")
# Warning-clean first-party sources; ImGui's own .cpp are vendored and silenced (mirrors the loader).
target_compile_options(example_mod PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CUBE_WARNING_FLAGS}>)
# -Wno-format-truncation (GCC/mingw only): the event console writes deliberately bounded,
# truncatable display lines. MSVC has no equivalent warning to silence.
if(NOT MSVC)
  target_compile_options(example_mod PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-format-truncation>)
endif()
set_source_files_properties(${IMGUI_SOURCES} PROPERTIES COMPILE_OPTIONS "${CUBE_NO_WARNING_FLAGS}")
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

# d3d9: ImGui DX9 backend. dwmapi/imm32: ImGui Win32 backend.
target_link_libraries(example_mod PRIVATE d3d9 dwmapi imm32)
if(NOT MSVC)
  target_link_options(example_mod PRIVATE -static -static-libgcc -static-libstdc++)
endif()
