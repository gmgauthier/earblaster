# EarBlaster

A media player **for The Lunduke Computer Operating System (LCOS)**.

![EarBlaster v1.0 window mock](brand/ui-reference.svg)

This is the first application intended exclusively for LCOS. It is not a port of an existing Linux player and it is not LCOS house software. It is a third-party app written to live on that desktop: XFCE, XLibre, Clearlooks chrome, no systemd, no online account, no AI features.

More LCOS-only applications will follow this one.

LCOS itself: [https://github.com/BryanLunduke/LCOS](https://github.com/BryanLunduke/LCOS)

## What it is

EarBlaster is a single-window player in the Windows Media Player 7 shape.

- gtkmm-3.0 + GTK3 CSS + Meson
- GStreamer 1.0 `playbin` for audio and video
- One skin (`lcos`), navy well + ice-white neon
- Spinning single-ring lightning mark while a track plays
- Embedded or sidecar album art overlays the mark
- Local files only in the default build

It borrows LCOS colours. It does **not** use Bryan Lunduke’s official seal. The product mark is the ring-and-bolt; the wordmark is the EARBLASTER pill.

## Status

M0 scaffold is in the tree: Meson + gtkmm-3 window, static `SealView`, menus, empty playlist. No GStreamer yet.

| File | What |
|---|---|
| [DEVELOPMENT.md](DEVELOPMENT.md) | Locked decisions, architecture, milestones M0–M6 |
| [brand/](brand/) | Official marks and the UI reference |

v1.0 is M0 through M6 in the development plan: window, spin, sound, playlist, cover/video, EQ, `.deb`.

## Brand

Palette: navy `#0B1D38`, ice `#E8F2FF`, client gray `#E6E6E1`.

- `brand/mark-ring-bolt.svg` — spinning well mark
- `brand/icon-tile.svg` — desktop / menu icon
- `brand/lockup-pill.svg` — About box lockup
- `brand/ui-reference.svg` — locked window mock (the screenshot above)

Grammar and file list: [brand/README.md](brand/README.md)

About box line, when we get there:

> EarBlaster — a media player for The Lunduke Computer Operating System.

## Target

LCOS (Devuan + XLibre + XFCE4). Build on any current Devuan/Debian box; ship as a `.deb` under the LCOS package layout.

## Build

```
sudo apt install build-essential meson ninja-build pkg-config g++ libgtkmm-3.0-dev
meson setup build
meson compile -C build
./build/earblaster
```

GStreamer and TagLib are not required until M2 / M4.

## License

[The Unlicense](https://unlicense.org). Public domain. See [UNLICENSE](UNLICENSE).
