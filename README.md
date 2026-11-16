<p align="center">
  <img src="docs/logo.svg" width="140" alt="printa logo">
</p>

<h1 align="center">printa</h1>

<p align="center">
  A lightweight screenshot tool for Linux (X11), written in C with GTK3 — in the spirit of Flameshot.
</p>

<p align="center">
  <img alt="MIT license" src="https://img.shields.io/badge/license-MIT-7B3CFF">
  <img alt="C + GTK3" src="https://img.shields.io/badge/C-GTK3-C13BD8">
  <img alt="X11" src="https://img.shields.io/badge/platform-X11-FF5A7E">
</p>

<p align="center">
  <a href="https://berdfandrade.github.io/printa/">Website</a>
</p>

---

<p align="center">
  <img src="docs/screenshot.png" alt="printa with a window selected: the rest of the screen dimmed, the selection size in the corner and the Save button just below">
</p>

## Features

- **One-click windows** — hover and the window under the cursor is highlighted; click to select it (frame included, minus the invisible shadow GTK windows draw around themselves).
- **Free regions** — drag to select any rectangle, in any direction.
- **Frozen screen** — the image is taken before the interface appears, so open menus and tooltips make it into the shot (pair it with `-d`).
- **Save or copy** — save to `~/Pictures`, copy to the clipboard, or both; "Save as" takes PNG and JPEG.
- **Multi-monitor and HiDPI** — covers the whole desktop and saves at the screen's real resolution.
- **Small** — one binary, no background service, no configuration.

## Installation

Build dependencies:

| Distribution | Command |
| --- | --- |
| Debian / Ubuntu / Mint | `sudo apt install build-essential libgtk-3-dev` |
| Fedora | `sudo dnf install gcc make gtk3-devel libX11-devel` |
| Arch | `sudo pacman -S base-devel gtk3 libx11` |

Build and install (to `/usr/local/bin`; change it with `PREFIX=`):

```bash
git clone https://github.com/berdfandrade/printa.git
cd printa
make
sudo make install
```

To remove it: `sudo make uninstall`.

## Usage

```bash
printa          # capture now
printa -d 3     # wait 3 seconds before capturing
```

The screen freezes and dims. Then:

| Action | Result |
| --- | --- |
| Hover | Highlights the window under the cursor (dashed border) |
| Click | Selects that window |
| Drag | Selects a region |
| **Enter** or the **Save** button | Saves to `~/Pictures/Captura_<date>.png` and copies it |
| Double-click a window | Selects and saves in one go |
| **Ctrl+C** | Copies to the clipboard only |
| **Ctrl+S** | Opens "Save as" (`.png` or `.jpg`) |
| **Esc** or right-click | Cancels |

With nothing selected, **Enter** captures the whole screen.

The on-screen hints and file names are currently in Portuguese.

### In scripts

The saved file's path goes to stdout, and the exit code is `0` when something
was saved or copied and `1` when the capture was cancelled or failed:

```bash
file=$(printa) && notify-send "Screenshot saved" "$file"
```

### On the Print Screen key

Create a custom keyboard shortcut that runs `printa`:

- **Cinnamon:** System Settings → Keyboard → Shortcuts → *Add custom shortcut*.
- **GNOME (Xorg session):** Settings → Keyboard → Keyboard Shortcuts → *Custom Shortcuts*.
- **Anything else:** any global hotkey tool, such as `xbindkeys` or `sxhkd`.

If your desktop already binds Print Screen to its own screenshot tool, disable that shortcut first.

## How it works

1. On start, printa reads the whole X11 root window into a `GdkPixbuf` and
   collects the visible windows in the window manager's stacking order
   (`_NET_CLIENT_LIST_STACKING`), trimming the shadow of client-side-decorated
   windows (`_GTK_FRAME_EXTENTS`).
2. It shows a borderless window spanning every monitor, draws the frozen capture
   with a dark layer on top, and grabs the mouse and keyboard.
3. The selection crops the original pixbuf, so the result is exactly what was on
   screen, without printa's own interface.

### Layout

| File | Role |
| --- | --- |
| `printa.c` | Capture, GTK interface, events, saving and copying |
| `geometry.c` | Selection geometry: drag rectangle, window under the cursor, CSD shadow, button placement |
| `output.c` | Cropping the capture, format from the file extension, file name |
| `tests/test_printa.c` | Unit tests for `geometry.c` and `output.c` |
| `docs/` | The project website (GitHub Pages) |

## Tests

The tests use GLib's GTest and need no display:

```bash
make test
```

## Limitations

- **X11 only.** Wayland compositors don't let programs read the screen
  directly; there the capture comes out blank (printa warns about it).
- **No annotations.** No arrows, text or blur like Flameshot: the focus is on
  capturing fast.
- **Clipboard.** The copied image stays available after printa exits only if a
  clipboard manager is running (Cinnamon and GNOME have one).

## License

[MIT](LICENSE) © 2026 Bernardo Andrade
