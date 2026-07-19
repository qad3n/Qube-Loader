#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

MOD_DIR="$(pwd)"
BUILD_DIR="$MOD_DIR/build"
GAME_DIR="${GAME_DIR:-$(cd .. && pwd)/cube_game}"
GAME_EXE="${GAME_EXE:-Cube.exe}"
WINE="${WINE:-wine}"
DLL_NAME="cube_mod.dll"
LOG_FILE="$BUILD_DIR/cube_mod.log"
INJECT_RETRIES=30
INJECT_DELAY=1

command -v "$WINE" >/dev/null || { echo "run: wine not found (set WINE to your wine binary)" >&2; exit 1; }

./build.sh

[ -f "$GAME_DIR/$GAME_EXE" ] || { echo "run: game not found at $GAME_DIR/$GAME_EXE (set GAME_DIR/GAME_EXE)" >&2; exit 1; }
[ -f "$BUILD_DIR/$DLL_NAME" ] || { echo "run: $DLL_NAME missing after build" >&2; exit 1; }
[ -f "$BUILD_DIR/inject.exe" ] || { echo "run: inject.exe missing after build" >&2; exit 1; }

GAME_PID=""
TAIL_PID=""
cleanup()
{
    [ -n "$TAIL_PID" ] && kill "$TAIL_PID" 2>/dev/null || true
    [ -n "$GAME_PID" ] && kill "$GAME_PID" 2>/dev/null || true
    pkill -f "$GAME_EXE" 2>/dev/null || true
}
trap cleanup EXIT INT TERM HUP

rm -f "$LOG_FILE"
echo "run: launching $GAME_EXE under wine"
( cd "$GAME_DIR" && WINEDEBUG="${WINEDEBUG:--all}" "$WINE" "$GAME_EXE" ) &
GAME_PID=$!

echo "run: waiting for $GAME_EXE process"
for _ in $(seq "$INJECT_RETRIES"); do
    pgrep -f "$GAME_EXE" >/dev/null && break
    sleep "$INJECT_DELAY"
done

echo "run: injecting $DLL_NAME"
injected=0
for _ in $(seq "$INJECT_RETRIES"); do
    if ( cd "$BUILD_DIR" && "$WINE" inject.exe "$GAME_EXE" "$DLL_NAME" ); then
        injected=1
        break
    fi
    sleep "$INJECT_DELAY"
done
[ "$injected" -eq 1 ] || { echo "run: injection failed" >&2; exit 1; }

echo "run: injected. tailing $LOG_FILE (Ctrl-C to stop and close the game)"

colorize()
{
    local g=$'\033[90m' r=$'\033[91m' y=$'\033[93m' b=$'\033[94m' w=$'\033[97m' x=$'\033[0m'
    sed -u -E \
        -e "s/^(\[[^]]*\]) (\[ERROR\]) (\[[^]]*\]): (.*)$/${g}\1${x} ${r}\2${x} ${r}\3:${x} ${r}\4${x}/" \
        -e "s/^(\[[^]]*\]) (\[WARN\]) (\[[^]]*\]): (.*)$/${g}\1${x} ${y}\2${x} ${y}\3:${x} ${y}\4${x}/" \
        -e "s/^(\[[^]]*\]) (\[INFO\]) (\[[^]]*\]): (.*)$/${g}\1${x} ${b}\2${x} ${b}\3:${x} ${w}\4${x}/" \
        -e "s/^(\[[^]]*\]) (\[DEBUG\]) (\[[^]]*\]): (.*)$/${g}\1${x} ${g}\2${x} ${g}\3:${x} ${w}\4${x}/" \
        -e "s/^(\[[^]]*\]) (\[TRACE\]) (\[[^]]*\]): (.*)$/${g}\1${x} ${g}\2${x} ${g}\3:${x} ${w}\4${x}/"
}

if [ -n "${NO_COLOR:-}" ] || [ ! -t 1 ]; then
    tail -F "$LOG_FILE" &
else
    tail -F "$LOG_FILE" > >(colorize) &
fi
TAIL_PID=$!

while pgrep -f "$GAME_EXE" >/dev/null; do
    sleep "$INJECT_DELAY"
done

echo "run: $GAME_EXE exited, stopping"
