# Directory map

Qube-Loader (CWAML): a mod loader for the 2013 alpha of Cube World. Inject a 32-bit DLL, drop mods
into `mods/`, and each mod gets a clean C/C++ API instead of raw memory. Design rule: "thin mod, fat
loader" (all offsets, pointer chains, guarded memory, and hooking live in the loader; a mod only sees
named objects). For the narrative overview and mod-authoring guide, read [README.md](README.md); this
file is the fast "where does X live" index.

## Top level
```
CMakeLists.txt        umbrella build (add_subdirectory modloader + example_lib + example_mod)
build.sh  build.bat   build on Linux (mingw cross) / Windows (MSVC)
run.sh    run.bat      build, launch Cube.exe, inject early, tail the log
cmake/                mingw cross toolchain file (Linux only)
cube_mod.ini.sample   sample loader config
modloader/            the loader DLL (cube_mod.dll), its injector, and the public SDK
example_mod/          full example mod: an ImGui menu exercising the whole API
example_lib/          minimal headless companion mod (publishes an inter-mod service; smallest template)
```

## The loader: `modloader/`
Layered, each layer talking only to the one below it. See the ASCII layer diagram in README ("How it
works").
```
modloader/
  CMakeLists.txt        builds cube_mod.dll (globs src/) + inject.exe (injector/), vendors MinHook + ImGui
  injector/inject.cpp   inject.exe: a standalone LoadLibrary injector
  include/minhook/       MinHook inline-hook engine (git submodule, loader only)
  sdk/                  the public mod SDK (see below)
  src/
    dllmain.cpp         DLL entry, environment probe, boot/eject
    core/               infrastructure: log, config, crash/faultguard, guarded memory (mem), ini, paths, iat
    game/               the ONLY layer that knows addresses: offsets.h + per-domain readers/writers
      gamehooks/        mod-facing game-function hooking (builtin/ hooks, hookbus dispatch, rawpool)
    hooks/              mechanism only: D3D9 EndScene/Reset, the MinHook detour owner, WndProc, input block
    loader/             mod management (kept distinct from src/core to avoid the modloader/ name echo)
      core/             mod discovery + loading, registry, lifecycle, deps/conflict, services, per-mod
                        config/storage/locale/assets, the host-to-mod event bus, writeguard
      game/             gameevents: per-frame event sourcing (diffs game state, emits events to mods)
    api/                the CubeApi bridge: one file per domain; bridge.h holds the shared reducers
    overlay/            the loader-owned ImGui overlay (context + DX9/Win32 backends + lifecycle + CubeOverlayApi)
    util/               small header-only helpers: field (guarded field IO), math, fmt, guard, inflight
```

Note on two `core/` dirs: `src/core/` is loader infrastructure; `src/loader/core/` is the
mod-management subsystem. Includes disambiguate by prefix (`"core/log.h"` vs `"loader/core/events.h"`).
The C++ `namespace modloader` is loader-wide (not tied to the `loader/` folder).

## The SDK: `modloader/sdk/`
What a mod compiles against. Two layers behind two umbrella headers:
```
sdk/
  cube_sdk.h            umbrella for the raw versioned C ABI; includes cube_sdk/*.h
  cube_sdk/             core.h, enums.h, types.h, apis.h, api.h, events_hooks.h (the C ABI, per concern)
  cube_mod.hpp          umbrella for the ergonomic C++ layer (this is what a mod includes); includes cube/*.hpp
  cube/                 one header per domain: hero, world, entity, pet, items, view, session, selection,
                        combat/stun (in hero.hpp), events, hookcall, services, config, storage, locale,
                        assets, menu, logger, common (Vec3/NamedValue)
  imgui/                Dear ImGui (git submodule); the loader owns the context, mods build it core-only
  cube_imgui.cmake      one-line helper a mod uses to compile ImGui core for its own build
```

## Naming glossary (game term -> C ABI struct -> C++ SDK class)
The loader is deliberately layered, so one concept can carry three names. This is intentional: the
`game/` layer stays faithful to the reverse-engineered class names, the C ABI is the stable wire
format, and the SDK class is the ergonomic name a mod sees. Do not "unify" these; they are aliases by
design, and renaming the ABI/SDK names breaks the versioned mod ABI.

| Concept                | game/ term        | C ABI struct     | SDK class (namespace cube) |
|------------------------|-------------------|------------------|----------------------------|
| local player           | Creature (player) | CubePlayer       | Hero (alias Player)        |
| any other living thing | Creature          | CubeEntity       | Entity                     |
| tamed companion        | Creature (pet)    | CubePet          | Pet                        |
| combat block           | -                 | CubeCombat       | Combat                     |
| stun/knockdown state   | -                 | CubeStun         | Stun                       |
| status buff            | -                 | CubeBuff         | Buff                       |
| world / area           | -                 | CubeWorld        | World                      |
| world structure        | -                 | CubeStructure    | Structure                  |
| item / equipment       | -                 | CubeItem         | Item                       |
| camera / display / audio | -               | CubeCamera/Display/Audio | Camera / Display / Audio |
| session / UI state     | -                 | CubeSession/CubeUi | Session / Ui             |

All three living-thing rows share the game's one `Creature` memory layout, so `game/creature.*` holds
the shared per-Creature reads/writes and the player/pet/entity readers build on it. In identifiers,
the full concept word wins (`health`, `cooldown`, `position`); `pos`/`cd` appear only as conventional
short-lived locals and `hp` only in compact log-line text.

## Where to look when adding to the API
1. `game/<domain>.{h,cpp}`: add the reader/writer that touches memory (offsets in `game/offsets.h`).
2. `sdk/cube_sdk/*`: declare the C ABI struct/field/function pointer (the versioned wire format).
3. `api/<domain>.cpp`: wire the bridge (`fillX`), reusing the reducers in `api/bridge.h`.
4. `sdk/cube/<domain>.hpp`: add the ergonomic C++ accessor a mod calls.
5. If it is an event, add the per-frame diff in `loader/game/gameevents.cpp`.
