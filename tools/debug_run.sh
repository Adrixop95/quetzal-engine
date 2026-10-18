#!/bin/sh
# Run the game with full diagnostics. Everything lands in debug/<timestamp>/ (and debug/<timestamp>.tar.gz).
# usage: tools/debug_run.sh [same options as run_aztec.sh]
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1
OUT="$ROOT/debug/$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUT"
{
  echo "== date";            date
  echo "== uname";           uname -a
  echo "== session env";     env | grep -E '^(XDG_|WAYLAND|DISPLAY|SDL_|GDK_|QT_|LANG|MESA|__GL)' | sort
  echo "== settings file";   cat game/aztec_settings.ini 2>&1
  echo "== git";             git log -3 --oneline 2>&1
  echo "== binary";          ls -l build/aztec_recomp 2>&1
  echo "== linked libs";     ldd build/aztec_recomp 2>&1 | grep -i -E 'sdl|libGL|egl|wayland|x11'
  echo "== gpu";             lspci 2>/dev/null | grep -i -E 'vga|3d|display'
  echo "== glxinfo";         glxinfo -B 2>&1 | head -25
  echo "== monitors (wlr-randr / xrandr)"; (wlr-randr 2>/dev/null || xrandr 2>/dev/null || echo "no randr tool")
  echo "== ffmpeg";          ffmpeg -version 2>&1 | head -1
  echo "== game dir";        ls -la game | head -50
  echo "== memory";          free -m
} > "$OUT/system.txt" 2>&1

echo "Running with diagnostics. Reproduce the problem, then close the game normally."
export RT_DEBUG=1 RT_DEBUG_FILE="$OUT/debug.log" RT_TRACE_INPUT=1
"$ROOT/run_aztec.sh" "$@" > "$OUT/stdout.txt" 2> "$OUT/stderr.txt"
RC=$?
echo "== exit code $RC" >> "$OUT/system.txt"
cp game/aztec_settings.ini "$OUT/settings_after.ini" 2>/dev/null
[ -f rt_missing_entries.txt ] && cp rt_missing_entries.txt "$OUT/"
tar czf "$OUT.tar.gz" -C "$ROOT/debug" "$(basename "$OUT")"
echo "exit code: $RC"
echo "debug log:  $OUT/debug.log"
echo "archive:    $OUT.tar.gz"
