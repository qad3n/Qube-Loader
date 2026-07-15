# modloader/include/ - the loader's third-party libraries

Third-party libraries the loader (`cube_mod.dll`) links against live here, as git
submodules (integrated projects, not copied into this repo). This is the loader's
own vendored-dependency directory, separate from a mod's `include/` (e.g.
`example_mod/include/` vendors ImGui). Libraries the loader needs but does not
expose to mods belong here.

Submodules keep the upstream source out of our tree: the repo records only a
pinned commit, and `git submodule update --init --recursive` (run automatically by
`build.sh`) fetches the actual files. Upgrading is `git -C <sub> checkout <tag>`
then committing the moved pointer, with no re-vendoring.

## Vendored

- `minhook/`: MinHook (`github.com/TsudaKageyu/minhook`, pinned to `v1.3.4`), the
  inline-hook engine backing every detour (the D3D9 EndScene/Reset hooks today,
  game-function hooks later). Consumed by `src/hooks/detour.cpp` via
  `#include "MinHook.h"`. `modloader/CMakeLists.txt` compiles only its five C
  sources and adds `minhook/include` to the include path (a full upstream checkout
  also ships tooling we do not build).
