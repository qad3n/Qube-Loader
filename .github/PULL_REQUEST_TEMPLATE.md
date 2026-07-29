<!--
The checklist below is the architecture contract from docs/ROADMAP.md ("Principles").
Delete any line that does not apply to your change rather than leaving it unchecked.
-->

## What this changes

<!-- One paragraph. What the mod-facing behavior is after this lands. -->

Closes #

## How it was verified

<!--
Headless builds prove nothing about rendering, input, or live game state. If this touches
D3D9, the overlay, input, or reads/writes real game memory, say what you ran and what you saw
under `./run.sh` with a character loaded, or state plainly that it is untested.
-->

## Checklist

- [ ] Builds on both toolchains (MSVC Win32 and mingw-w64 i686), or CI is green
- [ ] No address, offset, or pointer chain is visible to a mod. Everything memory-related stayed
      inside the loader (thin mod, fat loader)
- [ ] Every new read is `mem::readable`-gated and every new write is `mem::writable`-gated;
      any resolved Creature is vftable-validated before use
- [ ] If the ABI grew: the change is purely additive, `CUBE_ABI_VERSION` is bumped, and
      `CUBE_MIN_ABI_VERSION` is untouched, so older mods keep loading
- [ ] A new capability is reachable from both tiers: the C ABI in `cube_sdk.h` first, then the
      ergonomic wrapper in `cube_mod.hpp`
- [ ] New detours install only when a mod's subscription or API call needs them, and release when
      the last holder goes. With no mods loaded the game binary is still byte-identical to vanilla
- [ ] Any offset, struct layout, or field claim is backed by
      [CubeWorld-Reversal](https://github.com/qad3n/CubeWorld-Reversal) or other verifiable
      evidence, not inferred
- [ ] `docs/DIRECTORY_MAP.md` updated if files moved or were added
- [ ] `docs/ROADMAP.md` updated if a layer boundary or phase status changed
