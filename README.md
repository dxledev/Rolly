# Rolly

A portable, standalone archive manager written in C++20 with Qt Widgets and
libarchive. Open, browse, create, extend, trim and extract archives in a small,
quiet, resizable native window. Rolly is themed entirely from its command line,
and an open window repaints the moment its colors change.

- **Formats.** Read zip, 7z, tar (gz, bz2, xz, zst), rar, iso, cpio, deb, rpm,
  cab and more. Create and modify zip, 7z, tar, tar.gz, tar.bz2, tar.xz and
  tar.zst. Rar and the other read-only formats are browsed and extracted only.
- **Safe extraction.** Entries that try to leave the destination (`../`,
  absolute paths) are refused, and links inside an archive are never followed
  out of it. Existing files are only replaced after you confirm.
- **Gentle on memory.** Listings stream headers and never load archive contents;
  editing rewrites to a temporary file beside the archive and replaces the
  original only on success. Cancelling leaves the original untouched.
- **Everything inside the window.** Confirmations, passwords and progress appear
  as in-window dialogs over a soft blur. File choosing uses your desktop's own
  file chooser through the standard portal, so it matches the rest of your system.
- **Flat, minimal Lo-Fi look.** No gradients; icons are drawn as single-color
  paths and take the theme.

## Build and run

Requires CMake 3.21+, a C++20 compiler, libarchive, and Qt 6.7+ (Core, Gui,
Widgets, DBus; Test for the test suite). On Linux, install Qt's Wayland platform
plugin to run natively on Wayland.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/rolly [archive ...]
```

Install for your own account (binary, launcher entry, icon):

```bash
bash scripts/install-desktop.sh --dry-run
bash scripts/install-desktop.sh
```

The installer accepts `--prefix`, `--config-dir`, `--apps-list` and
`--follow PALETTE` (an absolute path). For a conventional package use
`cmake --install` with your preferred prefix.

## Using it

| Action | Keys |
| --- | --- |
| Open archive | Ctrl+O, or drop one on the window |
| New archive | Ctrl+N, or drop files on an empty window |
| Extract (selection, or everything) | Ctrl+E |
| Add files or a folder | Ctrl+Shift+A, or drop onto an open archive |
| Delete selection | Delete |
| Details | Alt+Enter |
| Up a folder | Backspace or Alt+Up |
| Close archive | Ctrl+W |

Double-click a folder to enter it, or a file to unpack it temporarily and open
it. Name a new archive with the extension you want (`.zip`, `.7z`, `.tar.gz`,
`.tar.xz`, `.tar.bz2`, `.tar.zst`); anything else becomes `.zip`.

## Your colors

Rolly keeps its palette in `$XDG_CONFIG_HOME/rolly/theme.json` (usually
`~/.config/rolly/theme.json`). Every theme command rewrites that file
atomically; open windows watch it and reload at once. A change that does not
validate is rejected and the last good colors stay.

```bash
rolly theme show
rolly theme set 'accent=#a9c7b0' 'surface=#1e2a2e'
rolly theme set 'background=#e00e1e36'
rolly theme set 'background=Qt.rgba(14 / 255, 30 / 255, 54 / 255, 0.88)'
rolly theme import /absolute/path/palette.json
rolly theme follow file /absolute/path/palette.json
rolly theme follow symlink /absolute/path/to/palette-or-directory
rolly theme sync
rolly theme preset lofi
```

Colors: `background surface surface_alt text muted accent accent_text border
selection danger success`.

- Six-digit `#RRGGBB` is opaque. Eight-digit colors are **alpha first**,
  `#AARRGGBB`, so `#e00e1e36` is RGB `#0e1e36` at alpha `e0`. `Qt.rgba(r, g, b, a)`
  with 0–1 channels or `n / 255` fractions is accepted too. Alpha is kept when
  the compositor supports a translucent window; otherwise only the alpha is
  dropped and the RGB channels are untouched.
- `import` copies a palette once. `follow` keeps tracking a file, or a symlink
  and whatever it points at, so replacing the target recolors Rolly live.
  Supported palettes: Rolly JSON, generic color JSON, QML color properties, and
  terminal color configs.
- `--dry-run` prints the proposed theme without writing. `--config-dir DIR` or
  `ROLLY_CONFIG_DIR` selects another configuration.

Set `ROLLY_REDUCED_MOTION=1` to skip the dialog animation.

## Launcher icon

The icon in docks and launchers is a fixed, full-color picture of what the app
handles (a packed crate). It is deliberately not themed.
