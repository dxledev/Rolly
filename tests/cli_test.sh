#!/usr/bin/env bash
set -euo pipefail
rolly="$1"
dir="$(mktemp -d)"
trap 'rm -rf "$dir"' EXIT
run() { "$rolly" --config-dir "$dir" "$@"; }

run theme set 'background=#e00e1e36' >/dev/null
run theme show | grep -q '"background": "#e00e1e36"'
run theme set 'background=Qt.rgba(14 / 255, 30 / 255, 54 / 255, 0.88)' >/dev/null
run theme show | grep -q '"background": "#e00e1e36"'
before="$(cat "$dir/theme.json")"
if run theme set 'accent=notacolor' 2>/dev/null; then echo "invalid color accepted" >&2; exit 1; fi
[[ "$before" == "$(cat "$dir/theme.json")" ]]
run theme set 'accent=#112233' --dry-run | grep -q '#112233'
grep -q '#112233' "$dir/theme.json" && { echo "dry run wrote"; exit 1; }
palette="$dir/palette.json"
printf '{"colors":{"background":"#101010","text":"#eeeeee","accent":"#ff8800"}}' > "$palette"
run theme follow file "$palette" >/dev/null
run theme show | grep -q '"accent": "#ff8800"'
printf '{"colors":{"background":"#101010","text":"#eeeeee","accent":"#00ff88"}}' > "$palette"
run theme sync >/dev/null
run theme show | grep -q '"accent": "#00ff88"'
"$rolly" --help | grep -qi 'archive manager'
if "$rolly" --help | grep -qiE 'quickshell|caelestia|noctalia|waybar|hyprland'; then echo "help names a shell" >&2; exit 1; fi
echo ok
