# EarBlaster development plan

A gtkmm-3 + GStreamer media player for LCOS. WMP 7 layout, one LCOS-flavoured skin, third-party branding.

Reference window: `brand/ui-reference.svg`  
Marks: `brand/README.md`  
License: The Unlicense (`UNLICENSE`)  
Repo: https://github.com/gmgauthier/earblaster

## Status (2026-09-08)

**M0 is done.** First compile and run on Debian 13 (Trixie) aarch64, XFCE on X11.

- Meson 1.7 + gcc 14.2 + gtkmm-3.0 3.24.10
- Window opens the locked layout: menus, navy well, ring + bolt, EARBLASTER pill, transport stubs, seek/volume, empty playlist, status bar
- About dialog loads `brand/lockup-pill.svg` and the guest phrasing
- Work-machine window chrome may be Adwaita/dark; that is host theme, not the skin. Visual target remains XFCE + Clearlooks-Phenix on XLibre

Next: **M1 — Spin.**

## 1. Locked decisions

| Decision | Choice |
|---|---|
| Product | Original app, not a Qmmp/Audacious rebrand |
| Look | Windows Media Player 7: one decorated window, playlist pane, vis well, transport |
| Toolkit | C++17, gtkmm-3.0, GTK3 CSS, Meson |
| Playback | GStreamer 1.0 `playbin` |
| Tags / art | TagLib first; GStreamer `GST_TAG_IMAGE` fallback; sidecar `folder.jpg` / `cover.jpg` / `AlbumArt.jpg` |
| Skin | One skin: GTK CSS + Cairo `SealView`. Not `.wsz` |
| Brand | Navy + ice neon, EARBLASTER pill, single-ring lightning mark. Not Bryan’s LCOS seal |
| Network | None in the default build |
| Init / session | No systemd dependency. ALSA or Pulse via playbin. MPRIS optional later |
| License | The Unlicense |

## 2. Window

```
+---------------------------------------------------------------+
| File  Edit  View  Play  Tools  Help                           |
+------------------------------+--------------------------------+
|  navy well                   |  #  Title        Artist  Time  |
|    [ spinning ring + bolt ]  |                                |
|    [ EARBLASTER pill ]       |                                |
|  [<] [>] [||] [■] [>>]       |                                |
|  seek ==========  vol ====   |                                |
+------------------------------+--------------------------------+
| Stopped — 0:00 / 0:00                                         |
+---------------------------------------------------------------+
```

Well stack, front to back:

1. Video sink — current URI is video
2. Cover pixbuf — embedded or sidecar art
3. Spinning ring + upright bolt
4. Navy `#0B1D38`

Pill is a caption under the circle. Hidden while cover or video is showing.

Motion: 6–10 RPM while `GST_STATE_PLAYING`. Freeze on pause. Angle 0 when stopped. Drive with `Gtk::Widget::add_tick_callback`.

Cover: contain (letterbox), ~8–12 px inset, never stretch. Cache the scaled pixbuf on size-allocate.

## 3. Architecture

```
earblaster
├── brand/                 official marks (this tree)
├── data/
│   ├── earblaster.desktop
│   ├── icons/
│   └── skin/lcos/
│       └── lcos.css
├── src/
│   ├── main.cpp
│   ├── application.{hpp,cpp}
│   ├── main_window.{hpp,cpp}
│   ├── seal_view.{hpp,cpp}      Cairo well
│   ├── player.{hpp,cpp}         playbin wrapper
│   ├── playlist.{hpp,cpp}       model + M3U
│   ├── cover_art.{hpp,cpp}      TagLib + sidecar
│   ├── eq_window.{hpp,cpp}      10-band dialog
│   └── about_dialog.{hpp,cpp}
├── meson.build
└── DEVELOPMENT.md
```

### `Player`

Wrap one `GstElement* playbin`.

- `open(uri)`, `play()`, `pause()`, `stop()`, `seek(ns)`, `set_volume(0..1)`
- Signals (sigc): `state_changed`, `position_changed`, `eos`, `error`, `tags_changed`
- Query duration/position on a 250 ms timeout only while playing
- Audio URI: default sink. Video URI: `gtksink` / `gtkglsink` widget handed to `SealView`
- EQ: bin `equalizer-10bands` on `audio-filter` when the dialog exists

### `SealView` : `Gtk::DrawingArea`

```cpp
void set_playing(bool);
void set_cover(const Glib::RefPtr<Gdk::Pixbuf>&); // empty clears
void set_video_widget(Gtk::Widget*);              // nullptr restores Cairo
```

`on_draw`: navy, rotated ring, upright bolt, optional cover.

Cairo notes from M0 (cairomm-1.0 on Trixie):

- Use `begin_new_sub_path()`, not `new_sub_path()`
- Pill lettering: `select_font_face` + `show_text`. Do not leave a text path and `fill()` after `stroke()` of the capsule — the path is already consumed

### `CoverArt`

```cpp
Glib::RefPtr<Gdk::Pixbuf> load(const std::string& path);
```

Order: TagLib front cover → first embedded picture → sidecar names in the file’s directory → empty.

### Playlist

`Gtk::ListStore`: filename, title, artist, duration, uri. Drag-drop files and `.m3u`. Double-click plays. Shuffle / repeat flags live on `Player`.

## 4. Skin (`data/skin/lcos/lcos.css`)

Stock GTK3 widgets. Clearlooks / Phenix keeps the window decorations.

- Client `#E6E6E1`
- TreeView selected row: steel blue, not Adwaita orange
- Scales and buttons: leave mostly to the desktop theme so it sits on XFCE
- `.earblaster-well` background `#0B1D38`

No custom title bar.

Dev machine (Debian Trixie + i3/Regolith or XFCE): compile is enough with `libgtkmm-3.0-dev`. Widget look can be forced with `GTK_THEME=Clearlooks-Phenix` if the engine is installed. Full title-bar fidelity needs xfwm4 + that theme (LCOS VM or an XFCE session). GStreamer packages are not required until M2.

## 5. Milestones

### M0 — Repo and window — **done 2026-09-08**

Meson project links gtkmm-3.0. `MainWindow`, menus, CSS load, `SealView` with a static ring + bolt and a static pill. About dialog uses `brand/lockup-pill.svg`. `.desktop` uses the icon tile.

Done when: `ninja && ./earblaster` opens the locked layout on XFCE.

Verified: Debian 13 aarch64, XFCE/X11. Layout, well, pill text, About lockup all present. Transport and Open File remain stubs.

### M1 — Spin (days)

Tick callback. `set_playing(true)` from a dummy toolbar toggle (Play / Pause is enough). Confirm the ring rotates and the bolt stays upright. Freeze on pause. Reset angle on stop.

### M2 — Sound (end of week 1)

`Player` + Open File. Transport wired. Seek and volume. Status bar `mm:ss / mm:ss`. Formats via playbin (MP3, Ogg, FLAC, WAV at minimum).

### M3 — Playlist (week 2)

ListStore, add files/folder, remove, drag reorder, M3U load/save, EOS → next, shuffle, repeat.

### M4 — Cover + video (week 2)

`CoverArt` into `SealView`. Pill hides when cover is set. Video URI swaps in `gtksink`.

### M5 — EQ + polish (week 3)

10-band dialog, persist bands in `~/.config/earblaster/earblaster.ini` (Glib::KeyFile). Window size/position. Keyboard: Space play/pause, arrows seek, Del remove.

### M6 — Package (week 3)

`lcos-packages/earblaster` Debian layout. `/usr/bin/earblaster`, icons under `hicolor`, skin under `/usr/share/earblaster/skin/lcos`. No daemon.

**v1.0 is M0–M6.**

### Later (not v1.0)

Spectrum ring outside the neon ring, playlist cover column, CUE sheets, MPRIS, gapless, CDDA, WinAmp skins.

## 6. Tooling (Devuan Excalibur / Debian Trixie)

M0 (current):

```
sudo apt install \
  build-essential meson ninja-build pkg-config \
  g++ \
  libgtkmm-3.0-dev
```

From M2:

```
sudo apt install \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly \
  gstreamer1.0-libav gstreamer1.0-gtk3 \
  libtag1-dev
```

Optional theme check on a non-LCOS box:

```
sudo apt install gtk2-engines gtk2-engines-pixbuf
# plus a Clearlooks-Phenix GTK3 theme if packaged
GTK_THEME=Clearlooks-Phenix ./build/earblaster
```

Build:

```
meson setup build
meson compile -C build
./build/earblaster
```

## 7. Packaging notes for LCOS

- Match `lcos-updates`: Meson + a small `debian/` directory. License is The Unlicense.
- Binary never runs as root
- Depends on GTK3, GStreamer good/ugly/libav, taglib
- Recommends the LCOS Clearlooks theme; do not vendor a window manager theme
- AppImage is a fallback (LCOS 0.3 already fixed AppImage). Prefer `.deb`

## 8. Test matrix (v1.0)

- Open MP3 with APIC, MP3 without art, FLAC with picture, folder with only `folder.jpg`
- Video file uses the well; returning to audio restores ring or cover
- Pause freezes the ring; stop resets it
- Playlist EOS, shuffle, empty list, missing file
- Unplug Pulse → playbin should still find ALSA
- XFCE + Clearlooks on XLibre: decorations and CSS do not fight

M0 already covered: cold launch, About box, resize of the well, compile on aarch64 Trixie.

## 9. First code to write

M0 is in the tree and has been run.

Do not touch GStreamer until the ring spins. Next code is M1: `SealView::set_playing`, a tick callback, and dummy Play/Pause on the existing buttons.
