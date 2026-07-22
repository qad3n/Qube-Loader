# cube_imgui.cmake - shared Dear ImGui build helper for the loader and for mods.
#
# Strategy B (shared context): the LOADER owns the single ImGui context + the DX9/Win32 backends + the
# whole overlay lifecycle. A mod compiles ImGui CORE only and calls ImGui:: inside a draw callback; the
# cube::Menu wrapper binds the mod's ImGui to the loader's live context + allocator over the C ABI. For
# that to be layout-compatible, BOTH sides must build the SAME ImGui sources - this one submodule.
#
# Usage (from a target's CMakeLists, after CUBE_NO_WARNING_FLAGS is set by the top-level project):
#   include("${CMAKE_SOURCE_DIR}/modloader/sdk/cube_imgui.cmake")
#   cube_add_imgui(my_mod)            # ImGui core only - what a mod needs
#   cube_add_imgui(cube_mod BACKENDS) # core + DX9 + Win32 backends - what the loader needs
#
# ImGui headers are added as SYSTEM includes so third-party warnings stay out of first-party builds,
# and the ImGui .cpp are compiled with CUBE_NO_WARNING_FLAGS (silenced, mirroring MinHook in the loader).

set(CUBE_IMGUI_DIR "${CMAKE_CURRENT_LIST_DIR}/imgui")

function(cube_add_imgui target)
  cmake_parse_arguments(ARG "BACKENDS" "" "" ${ARGN})

  if(NOT EXISTS "${CUBE_IMGUI_DIR}/imgui.h")
    message(FATAL_ERROR "ImGui submodule missing at ${CUBE_IMGUI_DIR}. "
      "Run: git submodule update --init --recursive")
  endif()

  set(_imgui_srcs
    "${CUBE_IMGUI_DIR}/imgui.cpp"
    "${CUBE_IMGUI_DIR}/imgui_draw.cpp"
    "${CUBE_IMGUI_DIR}/imgui_tables.cpp"
    "${CUBE_IMGUI_DIR}/imgui_widgets.cpp")

  target_include_directories(${target} SYSTEM PRIVATE "${CUBE_IMGUI_DIR}")

  if(ARG_BACKENDS)
    list(APPEND _imgui_srcs
      "${CUBE_IMGUI_DIR}/backends/imgui_impl_dx9.cpp"
      "${CUBE_IMGUI_DIR}/backends/imgui_impl_win32.cpp")
    target_include_directories(${target} SYSTEM PRIVATE "${CUBE_IMGUI_DIR}/backends")
    # DX9 backend: d3d9. Win32 backend: dwmapi + imm32.
    target_link_libraries(${target} PRIVATE d3d9 dwmapi imm32)
  endif()

  target_sources(${target} PRIVATE ${_imgui_srcs})
  set_source_files_properties(${_imgui_srcs} PROPERTIES COMPILE_OPTIONS "${CUBE_NO_WARNING_FLAGS}")
endfunction()
