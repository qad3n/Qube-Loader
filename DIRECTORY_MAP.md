# Reversal map

RE workspace for Cube World. Client `cube.exe`, `server`. Goal: reverse concepts here, port to modloader API.

## Where to look when reversing a concept
1. `decomp_source/{cube,server}/<category>/` readable reconstructed C++. Start here.
2. `dumps/{cube,server}/` ground truth. Verify against these, never guess.
3. `match/` confirm a function matches original binary byte for byte.

## decomp_source reconstructed C++
Categories per binary:
- cube: `ai audio control db entity game_misc render ui world`
- server: `ai db entity game_misc net world`

Per binary also: `README.md`, `GAP_ANALYSIS.md` (what's missing), `RENAMES.tsv`, `SANITIZED.tsv`, `attribution.tsv`, `include/`, `_library/` (lib code, skip).

## dumps raw RE artifacts (per binary: cube, server)
- `ghidra_decompiled_all.c` full decompilation
- `structs.h` recovered structs
- `ghidra_func_map.tsv` addr to name
- `callgraph_{nodes,edges}.tsv` call graph
- `string_xrefs.tsv`, `strings_offsets.txt` find code by string
- `exports.txt`, `imports.txt`, `rtti_classes.txt`, `sections.txt`, `pe_info.txt`
- `env/` external formats: `sqlite_schemas.txt`, `options.cfg.txt`, `plx_dat_format_notes.md`, `dll_notes.md`

## match MSVC recompile and diff
- `target/` 12,967 goal `.asm` per function
- `funcs/` reconstructed C++ per function (`<addr>_FUN_<addr>.cpp`)
- `tools/` `match_func.py`, `disasm_obj.py`, `asmnorm.py`
- `toolchain/` `cl.sh`, `verify.sh` (needs MSVC, see `PLACE_MSVC_HERE.md`)
- `README.md` workflow

## scratchpad transient
Ghidra/IDA projects, helper py scripts, `re_findings.md`, `ACCURACY_PLAN.md`, `refs/` (zlib/sqlite source for lib func ID). Ignore unless debugging tooling.
