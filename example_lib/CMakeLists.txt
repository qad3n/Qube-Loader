# Example library mod: a minimal headless companion that publishes an inter-mod service for
# example_mod to consume. No ImGui, no menu; also the smallest CUBE_MOD template. Builds into build/mods/.
file(GLOB_RECURSE EXAMPLE_LIB_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(example_lib SHARED ${EXAMPLE_LIB_SOURCES})
target_include_directories(example_lib PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/modloader/sdk")
target_compile_options(example_lib PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CUBE_WARNING_FLAGS}>)
set_target_properties(example_lib PROPERTIES
  PREFIX ""
  OUTPUT_NAME "example_lib"
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
if(NOT MSVC)
  target_link_options(example_lib PRIVATE -static -static-libgcc -static-libstdc++)
endif()
