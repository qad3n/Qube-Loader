# example_mod/include/ - this mod's headers

This directory holds the example mod's own headers (`menu/`, `mod_context.h`,
`features/`). It no longer vendors any third-party library.

## ImGui comes from the SDK, not here

The mod draws its menu with Dear ImGui, but ImGui itself is shipped by the SDK
(`modloader/sdk/imgui`, a git submodule pinned upstream), because the **loader
owns the single ImGui context and the DX9 + Win32 backends** (see the loader's
`src/overlay/` and `CubeOverlayApi`). A mod just registers a draw callback with
`mod.menu()` and writes ImGui code inside it - no hooking, no context, no
lifecycle.

`example_mod/CMakeLists.txt` calls `cube_add_imgui(example_mod)` (from
`modloader/sdk/cube_imgui.cmake`) to compile ImGui **core only** - the same
submodule the loader builds, so the shared context is layout-compatible. Adding
`imgui.h` to the include path is also what makes `cube_mod.hpp` auto-enable
`mod.menu()`. Run `git submodule update --init --recursive` (done automatically by
`build.sh`) to fetch the ImGui sources.
