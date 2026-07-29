# Cube World Alpha Mod Loader (Qube-Loader)

Qube-Loader is a mod loader for the 2013 alpha build of Cube World. Inject the 32-bit DLL, drop
your mods into a `mods/` folder, and each one gets a clean C/C++ API instead of raw memory.

With that API a mod can:

- **read** live game state (player, world, entities, items, camera, ...)
- **listen** for events (level up, took damage, entity spawned, ...)
- **intercept** game functions to change, cancel, or override them
- use **loader services**: config, save data, a manifest with dependencies, inter-mod
  messaging, localization, and asset overrides

The loader also runs a color-coded logger, captures the game's own debug output, and
installs a crash handler, so you can develop without a debugger attached.

## Status

An early proof of concept and a free-time hobby project, built for the alpha community.

- Documentation is limited to this README and the header comments in `modloader/sdk/`.
  Beyond that, the source is the reference.
- Features may be incomplete or not fully wired up. The API surface grows over time.
- The ABI is versioned but still moving, so a mod built against one development build
  can break on the next.
- The loader is built on reverse-engineered data (see below), which is not perfect, so
  wrong values and bugs happen.
- Development happens on Linux under Wine, not on Windows. Input, D3D9, and console
  behavior can differ on a real Windows machine.

Contributions are welcome. Covering the whole game in a mod API takes time.

## How it works

The design rule is "thin mod, fat loader." All offsets, pointer chains, struct layouts,
guarded memory access, and hooking live inside the loader. A mod only ever sees named
objects and helpers.

- A mod asks for the player with `mod.player()`. The loader has already resolved the
  pointer chain, validated the object, and read the fields.
- Live game state is read once per frame and handed to mods as plain data.
- Events are detected by the loader and delivered to any mod that subscribed.
- Hooks let a mod run on the game thread when a real game function is called, so it can
  cancel the call, change its arguments, or override its return value.

The loader is layered, each layer talking only to the one below it:

```
mods            your DLLs, #include "cube_mod.hpp" only, zero addresses
  SDK           sdk/cube_mod.hpp (ergonomic C++) over sdk/cube_sdk.h (versioned C ABI)
  api           the bridge: every mod call lands here, then dispatches to a reader/writer
  modloader     mod discovery + loading, the host-to-mod event bus, per-frame event sourcing
  hooks         mechanism only: D3D9 EndScene/Reset, the MinHook detour owner, WndProc, input
  game          the ONLY layer that knows addresses: offsets, per-domain readers/writers
  core          infrastructure: guarded memory, logging, crash handler, config
```

## Requirements

- A copy of the 2013 alpha `Cube.exe` (32-bit). The loader is Win32-only by nature; it
  hooks a Windows binary, so it does not run natively on Linux or macOS. On Linux it
  runs under Wine.
- CMake 3.16 or newer.
- A 32-bit C++17 toolchain:
  - Windows: Visual Studio 2019 or newer (the "Desktop development with C++" workload).
  - Linux: the mingw-w64 cross toolchain (`i686-w64-mingw32-g++`).
- Git (the loader and example mod pull MinHook and ImGui as submodules).

## Roadmap

- grow API coverage: more reads, writes, events, and built-in hooks for parts of the game
  not yet surfaced (the bulk of ongoing work)
- a Lua scripting tier, so mods can be written without a C++ compiler (maybe)
- server-side support (`Server.exe`), building on the client/server seam already in the API

## Structure

```
CMakeLists.txt        umbrella build (add_subdirectory modloader + example_lib + example_mod)
build.sh  build.bat   build on Linux (mingw) / Windows (MSVC)
run.sh    run.bat     build, launch Cube.exe, inject early, tail the log (Linux Wine / Windows)
cmake/                mingw cross toolchain (Linux only)
cube_mod.ini.sample   sample loader config

modloader/            the loader DLL (cube_mod.dll) and its injector
  src/core/           log, config, crash, guarded memory
  src/game/           offsets, per-domain game readers/writers, game-log capture
  src/game/gamehooks/ mod-facing game-function hooking (the intercept subsystem)
  src/hooks/          D3D9 + MinHook detour + render dispatch
  src/loader/         mod loading, registry, event bus, per-frame event polling
  src/api/            the CubeApi bridge (one file per domain)
  sdk/                the public mod SDK you compile against (split per domain internally)
    cube_sdk.h        umbrella for the raw versioned C ABI
    cube_sdk/         the C ABI split per concern (core, enums, types, apis, api, events_hooks)
    cube_mod.hpp      umbrella for the ergonomic C++ layer (this is what a mod includes)
    cube/             the C++ layer split per domain (player, world, creature, items, view, ...)
    imgui/            Dear ImGui (git submodule) - shipped with the SDK; the loader owns the
                      context + backends, mods build it core-only via cube_imgui.cmake
    cube_imgui.cmake  one-line helper a mod uses to compile ImGui core for its own build
  src/overlay/        the loader-owned ImGui overlay (context, backends, lifecycle, CubeOverlayApi)
  injector/inject.cpp inject.exe, a standalone LoadLibrary injector (built by modloader)
  include/minhook/    MinHook inline-hook engine (git submodule, loader only)

example_mod/          a full example mod: an ImGui menu exercising the whole API
example_lib/          a minimal headless companion mod: publishes an inter-mod service that
                      example_mod depends on, resolves, and messages. The smallest mod template.
```

## Your first mod

MinHook and ImGui are git submodules, so a plain `git clone` leaves them empty and the
build fails. Clone with submodules:

```
git clone --recurse-submodules https://github.com/qad3n/Qube-Loader
```

For a clone that already exists:

```
git submodule update --init --recursive
```

1. Create a folder for your mod with its own `CMakeLists.txt` that builds a 32-bit
   shared library and adds `modloader/sdk` to its include path. The example mod is a
   working template. For a menu, add `cube_add_imgui(<target>)` (from
   `modloader/sdk/cube_imgui.cmake`) - one line; omit it if your mod draws nothing.
2. In your source, include only `cube_mod.hpp` and write a `CUBE_MOD(...)` block:

   ```cpp
   #include "cube_mod.hpp"

   CUBE_MOD("Hello", "1.0.0", "you")
   {
       mod.log.info("hello from my first mod");
   }
   ```

3. Build it into a DLL and place that DLL in the `mods/` folder next to `cube_mod.dll`
   (the example builds into `build/mods/`). The loader discovers and loads every DLL in
   that folder on startup.
4. Inject the loader and watch `cube_mod.log`.

That is the whole contract: a DLL that exports `CubeMod_Init`, which the `CUBE_MOD`
macro generates for you. No offsets, no manual memory access.

## Core features and examples

### Read live game state

Player, world, creatures, items, camera, and more are all readable:

```cpp
CUBE_MOD("Reader", "1.0.0", "you")
{
    mod.eventListener().onFrame([&]
    {
        cube::Player player = mod.player();
        if (player.isAlive())
            mod.log.info("hp %.0f  pos %.1f,%.1f,%.1f",
                player.getHealth(),
                player.getPosition().x, player.getPosition().y, player.getPosition().z);
    });
}
```

### Write game state (guarded)

Stored values that can be safely changed get a setter. Writes are validated and guarded
by the loader:

```cpp
CUBE_MOD("God Mode", "1.0.0", "you")
{
    mod.eventListener().onFrame([&]
    {
        mod.player().setHealth(1000.0f); // refill every frame
    });
}
```

### Observe events

Events fire after the loader detects a change. Read-only, the game runs normally:

```cpp
CUBE_MOD("Level Watcher", "1.0.0", "you")
{
    mod.eventListener().onLevelUp([&](int newLevel)
    {
        mod.log.info("reached level %d", newLevel);
    });

    mod.eventListener().onDamaged([&](float damage)
    {
        mod.log.info("took %.0f damage", damage);
    });
}
```

### Draw an ImGui menu (overlay)

The loader owns the entire overlay: the D3D9 hook, the ImGui context, the DX9 + Win32
backends, the per-frame render, the toggle key, DPI scaling, device-reset recovery, and
the game input freeze. A mod registers a draw callback and writes ImGui inside it. There
is one shared context, so any number of mods can each draw their own menu.

```cpp
#include "cube_mod.hpp" // auto-enables mod.menu() when your build has ImGui (see below)

CUBE_MOD("My Menu", "1.0.0", "you")
{
    mod.setCapabilities(cube::Capability::Overlay);

    // INSERT toggles it by default; the game is frozen while it is open.
    mod.menu().window("My Menu", []
    {
        ImGui::Text("Hello from my mod");
        static bool god = false;
        ImGui::Checkbox("God mode", &god);
        if (ImGui::Button("Kill all")) { /* ... */ }
    });
}
```

`window(title, fn)` wraps your widgets in a titled window. For multiple windows or a
custom layout, use `mod.menu().onDraw(fn)` instead. Optional settings, all with sane
defaults: `setToggleKey(VK_F1)`, `setOpen(true)`, `setPassthrough(true)` (a display-only
HUD that leaves the game playable), `setUiScale(1.25f)`.

`mod.addMenu()` adds a further menu with its own toggle key, e.g. a HUD on F1 next to the
config panel on INSERT. The loader freezes the game while any interactive menu is open. A
menu whose draw keeps throwing is disabled on its own, so a broken mod never takes the
others down.

```cpp
cube::Menu& hud = mod.addMenu();
hud.setToggleKey(VK_F1).setPassthrough(true).window("HUD", [] { ImGui::Text("hp: ..."); });
```

One line in your `CMakeLists.txt` puts ImGui in your build. It compiles ImGui core against
the same submodule the loader ships, so the shared context is layout-compatible:

```cmake
include("${CMAKE_SOURCE_DIR}/modloader/sdk/cube_imgui.cmake")
cube_add_imgui(my_mod)
```

### Intercept game functions (hooks)

Hooks run on the game thread when a real function is called. The handler can cancel it,
change its arguments, or override its return. Keep hook handlers tiny:

```cpp
CUBE_MOD("Always Crit", "1.0.0", "you")
{
    // built-in hook: force every crit roll to succeed
    mod.eventHook().onCritRoll([](cube::HookCall& c) { c.setReturnInt(1); });

    // built-in hook: soften or ignore incoming melee hits (the damage amount is a float)
    mod.eventHook().onImpact([](cube::HookCall& c)
    {
        if (c.damage() > 50.0f)
            c.cancel(); // ignore big hits
        else
            c.setDamage(c.damage() * 0.5f); // halve small ones
    });

    // raw hook: an address you found yourself
    mod.eventHook().raw(0x004889e0, cube::CallConv::Thiscall, 0,
        [](cube::HookCall&) { /* ... */ });
}
```

### Guarded raw memory (escape hatch)

When you do hold a raw address, read or write it through the loader-guarded helpers.
They validate the page first and return a safe fallback instead of crashing:

```cpp
int hp = mod.read<int>(address); // 0 if the address is unmapped
bool wrote = mod.write<int>(address, 100); // false if blocked or unmapped
unsigned live = mod.rebase(0x00525a30); // static address -> live address
```

## Loader services

The loader hands each mod a set of services so it never hand-rolls file I/O, versioning,
or inter-mod plumbing. All are reached through `mod`.

### Manifest: identity, capabilities, dependencies

Declare who your mod is and what it needs. The loader keys per-mod state on the id, denies
any power you did not declare, and refuses to start a mod whose dependency is missing or
out of range:

```cpp
CUBE_MOD("My Mod", "1.2.0", "you")
{
    mod.setId("you.mymod");                 // stable id (keys config/storage/services)
    mod.setCapabilities(cube::Capability::Writes | cube::Capability::Overlay);
    mod.dependsOn("you.otherlib", "1.0.0"); // require another mod loads first
    mod.setPriority(10);                    // higher dispatches last in every reduce
    mod.setUpdateUrl("https://example.com/mymod"); // shown in the load banner (offline)
}
```

A mod that declares no capabilities is unrestricted (the trusted default); once it declares
any, every undeclared power is denied and logged. A mod that faults repeatedly is
auto-disabled in the registry so it stops crash-looping the game.

### Per-mod config and save storage

`mod.config()` is user-editable settings (an ini file); `mod.storage()` is mod-owned binary
save data. Both persist across restarts and are scoped to your mod:

```cpp
bool greet = mod.config().getBool("greet_on_load", true);
mod.config().setInt("log_level", 2);

int launches = mod.storage().getValue<int>("launches", 0) + 1;
mod.storage().putValue<int>("launches", launches);
mod.storage().setScope("world_1234");       // optional: namespace by save / world / character
```

### Inter-mod services and messaging

One mod publishes a service others resolve by name; mods send directed messages by mod id.
Consumers resolve at `onReady`, when every mod has loaded and dependencies are settled:

```cpp
// provider
mod.services().registerService("mymod.api", 1, &myVtable);
mod.services().onMessage([](cube::Message& m) { m.reply(m.id() + 1); });

// consumer
mod.eventListener().onReady([&]
{
    MyApi* api = mod.services().query<MyApi>("mymod.api", 1);
    int payload = 41;
    int reply = mod.services().sendMessage("you.mymod", 1, &payload, sizeof(payload));
});
```

### Lifecycle events

Beyond init and shutdown, react to load completion and world residency:

```cpp
mod.eventListener().onReady([&]{ /* all mods loaded + deps resolved */ });
mod.eventListener().onWorldEnter([&]{ /* entered a world from the title/menu */ });
mod.eventListener().onWorldExit([&]{ /* returned to the title/menu */ });
```

### Localization

Translate UI strings against per-mod `lang/<locale>.ini` files, with a live locale switch;
a missing key falls back to the supplied default:

```cpp
std::string title = mod.locale().translate("menu_title", "Menu");
mod.locale().setLocale("de");
```

### Asset overrides

Replace a game asset by its original filename key (requires the Assets capability and a
compatible game build). The loader re-encodes your bytes into the game's storage format:

```cpp
mod.assets().set("aim.png", pngBytes, pngSize);
bool has = mod.assets().has("aim.png");
mod.assets().remove("aim.png");
```

## Build

Windows (Visual Studio / MSVC), from a Developer Command Prompt:

```
build.bat
```

Linux (mingw cross-compile):

```
./build.sh
```

Both produce a 32-bit `build/cube_mod.dll`, `build/inject.exe`, and the sample
`build/mods/example_mod.dll`. Build type defaults to `Release`; set
`CUBE_MOD_BUILD_TYPE=Debug` to change it.

If you configure with CMake directly on Windows, pass `-A Win32` so the target is
32-bit. Do not use the mingw toolchain file there; it is only for the Linux build.

## Run

Use the run script. It builds, launches `Cube.exe`, injects the loader the moment the
game starts, and tails the live log:

- Linux (Wine): `./run.sh`
- Windows: `run.bat` (double-click, or run from a Developer Command Prompt)

**Inject at startup, not mid game.** The loader must hook the game as it launches, which
is what the run script does. Injecting into a game that is already in a world can
misbehave or crash.

Override the game location with `GAME_DIR` / `GAME_EXE`.

In game, `INSERT` opens the example mod's menu.

Manual injection, on either platform, right after launching the game and before loading a
world:

```
inject.exe Cube.exe path\to\cube_mod.dll
```

Any DLL injector works. A log is written to `cube_mod.log` next to the DLL. `END` in the
loader console unloads it.

Loader settings come from defaults, then `cube_mod.ini` next to the DLL, then
environment variables. See `cube_mod.ini.sample`.

## Related project: the reverse-engineering source

Qube-Loader is built in direct reference to a companion project I maintain:

  https://github.com/qad3n/CubeWorld-Reversal

That repository is dedicated to reversing the latest deprecated alpha build of Cube
World from 2013. Every offset, pointer chain, and struct layout the loader uses comes
from that work. Because it is decompiled and reconstructed rather than official, some
of it is approximate. If a value the loader exposes looks wrong, the reversal is the
place to check it, and corrections there flow back into the loader.

## License

Qube-Loader is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE)
for the full text.

Third-party components keep their own licenses: MinHook (BSD-2-Clause) and Dear ImGui
(MIT) are vendored as git submodules under their respective `include/` folders, each
with its own `LICENSE.txt`.
