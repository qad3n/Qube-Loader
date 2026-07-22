#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

BUILD_DIR="$PWD/build"
GAME_DIR="${GAME_DIR:-$(cd .. && pwd)/cube_game}"
GAME_EXE="${GAME_EXE:-Cube.exe}"
WINE="${WINE:-wine}"
DLL_NAME="cube_mod.dll"
LOG_FILE="$BUILD_DIR/cube_mod.log"
RETRIES=30

command -v "$WINE" >/dev/null || { echo "run: wine not found (set WINE)" >&2; exit 1; }

./build.sh

for f in "$GAME_DIR/$GAME_EXE" "$BUILD_DIR/$DLL_NAME" "$BUILD_DIR/inject.exe"; do
    [ -f "$f" ] || { echo "run: missing $f" >&2; exit 1; }
done

TAIL_PID=""
cleanup()
{
    [ -n "$TAIL_PID" ] && kill "$TAIL_PID" 2>/dev/null || true
    pkill -f "$GAME_EXE" 2>/dev/null || true
}
trap cleanup EXIT INT TERM HUP

rm -f "$LOG_FILE"
echo "run: launching $GAME_EXE"
( cd "$GAME_DIR" && WINEDEBUG="${WINEDEBUG:--all}" "$WINE" "$GAME_EXE" ) &

for _ in $(seq "$RETRIES"); do
    pgrep -f "$GAME_EXE" >/dev/null && break
    sleep 1
done

echo "run: injecting $DLL_NAME"
injected=0
for _ in $(seq "$RETRIES"); do
    ( cd "$BUILD_DIR" && "$WINE" inject.exe "$GAME_EXE" "$DLL_NAME" ) && { injected=1; break; }
    sleep 1
done
[ "$injected" = 1 ] || { echo "run: injection failed" >&2; exit 1; }

tail -F "$LOG_FILE" &
TAIL_PID=$!

while pgrep -f "$GAME_EXE" >/dev/null; do
    sleep 1
done
