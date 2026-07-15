# example_mod/include/ - this mod's headers and vendored libraries

This directory holds the example mod's own headers (`overlay.h`, `menu.h`,
`mod_context.h`) alongside the third-party libraries it links against, vendored
as git submodules (integrated projects, not copied into this repo).

Submodules keep the upstream source out of our tree: the repo records only a
pinned commit, and `git submodule update --init --recursive` (run automatically by
`build.sh`) fetches the actual files.

## Vendored

- `imgui/`: Dear ImGui (`github.com/ocornut/imgui`, pinned to `v1.92.8`), the
  immediate-mode UI the mod draws its debug menu with. The loader has no ImGui;
  each mod owns its own context, so there is no shared-context problem.
  `example_mod/CMakeLists.txt` compiles only the core plus the DX9 and Win32
  backends (`imgui_impl_dx9.cpp`, `imgui_impl_win32.cpp`); a full upstream checkout
  also ships every other backend and the `examples/`, which we do not build.
