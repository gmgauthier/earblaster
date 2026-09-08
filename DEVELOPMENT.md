# EarBlaster development plan

A gtkmm-3 + GStreamer **audio** player for LCOS. WMP 7 layout, one LCOS-flavoured skin, third-party branding. No video.

Reference window: `brand/ui-reference.svg`  
Marks: `brand/README.md`  
License: The Unlicense (`UNLICENSE`)  
Repo: https://github.com/gmgauthier/earblaster

## Status (2026-09-08)

**M3 is in the tree.** Playlist uses **New** vs **Add**. Compiled on Debian 13.

- New File/Folder/Playlist replaces the list and plays the first row
- Add File/Folder/Playlist and drag-drop append; transport stays put
- Prev/Next, EOS → next, Shuffle, Repeat (all). Edit → Remove / Delete
- Double-click plays that row. Save Playlist writes M3U

Next: **M4 — Cover** (TagLib + sidecars; well already stretch-fills `GST_TAG_IMAGE`).

## 1. Locked decisions

| Decision | Choice |
|---|---|
| Product | Original app, not a Qmmp/Audacious rebrand |
| Look | Windows Media Player 7: one decorated window, playlist pane, vis well, transport |
| Toolkit | C++17, gtkmm-3.0, GTK3 CSS, Meson |
| Playback | GStreamer 1.0 `playbin`, **audio only**. No video sink, no video codecs. VLC remains the LCOS video player. |
| Tags / art | TagLib first; GStreamer `GST_TAG_IMAGE` fallback; sidecar `folder.jpg` / `cover.jpg` / `AlbumArt.jpg` |
| Cover in well | One bead lap, then **stretch** the image to fill the entire well (no letterbox, no inset). Stop restores the mark. |
| Skin | One skin: GTK CSS + Cairo `SealView`. Not `.wsz` |
| Brand | Navy + ice neon, EARBLASTER pill, single-ring lightning mark. Not Bryan’s LCOS seal |
| Network | None in the default build |
| Init / session | No systemd dependency. ALSA or Pulse via playbin. MPRIS optional later |
| License | The Unlicense |
| Playlist verbs | Two commands, never mixed. **New** = replace the list and start the first (or only) row now. **Add** = append to the list and do not change transport. “Play” is transport only (`>`, Play menu). |

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

1. Cover pixbuf — after one bead revolution, stretch-filled to the well
2. Ring + upright bolt + tracer bead while playing (and during that first lap)
3. Navy `#0B1D38`

Pill is a caption under the circle. Hidden once cover is showing.

Motion: yellow bead, 1 rev/s, while `GST_STATE_PLAYING` and cover is not yet up. Freeze on pause. Angle 0 and cover hidden when stopped. Drive with `Gtk::Widget::add_tick_callback`.

Cover: **stretch** to the well’s pixel size (ignore aspect). Cache the scaled pixbuf on size-allocate. If tags arrive after the lap has finished, show immediately.

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
- Audio sink: default (Pulse or ALSA). `video-sink` is `fakesink` so a video file cannot open a picture
- File chooser and MIME filter: audio only (MP3, Ogg, FLAC, WAV, M4A at minimum)
- EQ: bin `equalizer-10bands` on `audio-filter` when the dialog exists

### `SealView` : `Gtk::DrawingArea`

```cpp
void set_playing(bool);                           // play vs pause; does not reset angle
void stop();                                      // hide bead, angle 0
void set_cover(const Glib::RefPtr<Gdk::Pixbuf>&); // empty clears
```

`on_draw`: if intro lap is done and a cover is set, stretch-paint it over the well; otherwise navy, ring, bolt, bead, pill.

Cairo notes from M0 (cairomm-1.0 on Trixie):

- Use `begin_new_sub_path()`, not `new_sub_path()`
- Pill lettering: `select_font_face` + `show_text`. Do not leave a text path and `fill()` after `stroke()` of the capsule — the path is already consumed

### `CoverArt`

```cpp
Glib::RefPtr<Gdk::Pixbuf> load(const std::string& path);
```

Order: TagLib front cover → first embedded picture → sidecar names in the file’s directory → empty.

### Playlist

`Gtk::ListStore`: filename, title, artist, duration, uri.

Two verbs, used everywhere (File menu, drag-drop). “Play” on the transport and Play menu means play/pause the current row only.

| Verb | List | Transport |
|---|---|---|
| **New** File / Folder / Playlist | replace | start the first new row immediately |
| **Add** File / Folder / Playlist | append | unchanged (keep playing, paused, or stopped) |

- **New File…** — multi-select; audio only; filename order; one file is a one-row list
- **New Folder…** — selected folder plus **one** level of subfolders (artist → albums). Audio only; filename order; play first. Skip hidden (`.`-prefixed) names.
- **Add File…** / **Add Folder…** — same scan rules, append
- Drag files or `.m3u` onto the pane = **Add**
- Double-click a row = play that row (list unchanged)
- Stop resets the well; it does not clear the list. New/Add is what changes membership
- EOS / Next walk the list; after the last row, stop (unless Repeat)

File menu:

```
New File…
New Folder…
New Playlist…
────────────────
Add File…
Add Folder…
Add Playlist…
────────────────
Save Playlist…
────────────────
Quit
```

No list-pane buttons in v1. Do not label anything "Open" or use "Play" for queue commands.

Shuffle / repeat flags live on `Player`.

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

### M1 — Spin — **done 2026-09-08**

Tick callback. Dummy Play / Pause / Stop. A yellow bead runs the circumference once per second so motion is visible on a uniform ring. Bolt stays upright. Freeze on pause. Hide bead and reset angle on stop.

Done when: Play shows the bead lapping the ring; Pause freezes it; Stop returns the well to rest. No GStreamer.

### M2 — Sound — **done 2026-09-08**

`Player` + Open File. Transport wired. Seek and volume. Status bar `mm:ss / mm:ss`. Audio formats via playbin (MP3, Ogg, FLAC, WAV at minimum). No video. Embedded cover via `GST_TAG_IMAGE` after one bead lap.

### M3 — Playlist — **done 2026-09-08**

ListStore. File menu uses **New** (replace + start) vs **Add** (append, leave transport). New/Add File (multi-select), New/Add Folder (recursive), M3U New/Add/Save. Drag-drop is Add. Double-click plays that row. Prev/Next, EOS → next, shuffle, repeat. Relabel M2 “Open File…” to “New File…”.

### M4 — Cover (week 2)

`CoverArt` into `SealView::set_cover`. After one bead lap, the image stretch-fills the well. Pill and mark hide. Stop restores the mark. No video widget.

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
  gstreamer1.0-libav \
  libtag1-dev
# gstreamer1.0-gtk3 (gtksink) is not used — audio only.
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
- Video files are rejected (chooser filter); playbin video-sink is fakesink
- Pause freezes the bead; stop hides it
- Playlist EOS, shuffle, empty list, missing file
- Unplug Pulse → playbin should still find ALSA
- XFCE + Clearlooks on XLibre: decorations and CSS do not fight

M0 already covered: cold launch, About box, resize of the well, compile on aarch64 Trixie.

## 9. First code to write

M0–M3 are in the tree.

Next code is M4: `CoverArt` with TagLib and sidecar `folder.jpg` / `cover.jpg`, feeding `SealView::set_cover` (stretch after one bead lap).
