#!/bin/sh
# Launch the recompiled game. Usage: ./run_aztec.sh [--res WxH] [--fullscreen] [--menu] [--no-video]
# In the game F9 opens the helper menu (display settings, helpers); settings live in game/aztec_settings.ini
# Example (widescreen): ./run_aztec.sh --res 1920x1080
DIR="$(cd "$(dirname "$0")" && pwd)"
while [ $# -gt 0 ]; do
  case "$1" in
    --res) export RT_RESOLUTION="$2"; shift 2;;
    --no-video) export RT_NO_VIDEO=1; shift;;
    --fullscreen) export RT_FULLSCREEN=1; shift;;
    --menu) export RT_MENU=1; shift;;
    *) echo "unknown option $1"; exit 2;;
  esac
done
exec "$DIR/build/aztec_recomp" "$DIR/game/Aztec.exe"
