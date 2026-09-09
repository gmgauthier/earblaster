# EarBlaster development plan

A gtkmm-3 + GStreamer **audio** player for LCOS. WMP 7 layout, one LCOS-flavoured skin, third-party branding. No video.

Reference window: `brand/ui-reference.svg`  
Marks: `brand/README.md`  
Install: `INSTALL.md`  
License: The Unlicense (`UNLICENSE`)  
Repo: https://github.com/gmgauthier/earblaster

## Status (2026-09-09)

**M0–M6 are in the tree.** Tagged **[0.1.1](https://github.com/gmgauthier/earblaster/releases/tag/v0.1.1)**.

| Area | What shipped |
|---|---|
| Window / skin | gtkmm-3, `SealView`, `lcos.css`, About lockup |
| Transport | playbin audio, seek, volume, status `mm:ss / mm:ss` |
| Playlist | New vs Add, folder + one album level, M3U, drop, GstDiscoverer |
| Cover | TagLib → sidecar → `GST_TAG_IMAGE`; stretch-fill after one bead lap |
| Polish | 10-band EQ, prefs, `earblaster.ini`, keyboard, single-instance |
| Package | `debian/`, `scripts/release.sh` → `.deb`, tarball, AppImage |

The plan called v1.0 “M0 through M6.” That feature set is what 0.1.x ships. Retag 1.0 after the `.deb` has lived on an LCOS box.

Next work is post-v1.0 (see §9), not another milestone in this sequence.

## 1. Locked decisions

| Decision | Choice |
|---|---|
| Product | Original app, not a Qmmp/Audacious rebrand |
| Look | Windows Media Player 7: one decorated window, playlist pane, vis well, transport |
| Toolkit | C++17, gtkmm-3.0, GTK3 CSS, Meson |
| Playback | GStreamer 1.0 `playbin`, **audio only**. No video sink, no video codecs. VLC remains the LCOS video player. |
| Tags / art | TagLib first; sidecar `folder.jpg` / `cover.jpg` / `AlbumArt.jpg`; GStreamer `GST_TAG_IMAGE` last |
| Cover in well | One bead lap, then **stretch** the image to fill the entire well (no letterbox, no inset). Stop restores the mark. |
| Skin | One skin: GTK CSS + Cairo `SealView`. Not `.wsz` |
| Brand | Navy + ice neon, EARBLASTER pill, single-ring lightning mark. Not Bryan’s LCOS seal |
| Network | None in the default build |
| Init / session | No systemd dependency. ALSA or Pulse via playbin. MPRIS optional later |
| License | The Unlicense |
| Playlist verbs | Two commands, never mixed. **New** = replace the list and start the first (or only) row now. **Add** = append to the list and do not change transport. “Play” is transport only (`>`, Play menu). |
| Versioning | `meson.build` is the source of truth (`0.1.1`). Debian changelog tracks the same upstream version. |

## 2. Window

```
+---------------------------------------------------------------+
| File  Edit  View  Play  Tools  Help                           |
+------------------------------+--------------------------------+
|  navy well                   |  Title        Artist  Time     |
|    [ spinning ring + bolt ]  |                                |
|    [ EARBLASTER pill ]       |                                |
|  [<<] [>] [||] [[] ] [>>]    |                                |
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

Motion: yellow bead, 1 rev/s, while playing and cover is not yet up. Freeze on pause. Angle 0 and cover hidden when stopped. Drive with `Gtk::Widget::add_tick_callback`.

Cover: **stretch** to the well’s pixel size (ignore aspect). Cache the scaled pixbuf on size-allocate. If tags arrive after the lap has finished, show immediately.

## 3. Architecture

```
earblaster
├── brand/                      official marks (this tree)
├── data/
│   ├── earblaster.desktop
│   ├── samples/                 git-only demo audio; excluded from meson dist
│   └── skin/lcos/lcos.css
├── debian/                     native package
├── scripts/release.sh          tarball / .deb / AppImage
├── src/
│   ├── main.cpp
│   ├── application.{hpp,cpp}    Gtk::Application + single-instance lock
│   ├── paths.{hpp,cpp}          SOURCE_ROOT / DATADIR / EARBLASTER_DATA
│   ├── main_window.{hpp,cpp}
│   ├── seal_view.{hpp,cpp}      Cairo well
│   ├── player.{hpp,cpp}         playbin + equalizer-10bands
│   ├── playlist.{hpp,cpp}       ListStore + M3U + GstDiscoverer
│   ├── cover_art.{hpp,cpp}      TagLib + sidecar
│   ├── settings.{hpp,cpp}       ~/.config/earblaster/earblaster.ini
│   ├── eq_window.{hpp,cpp}
│   ├── prefs_window.{hpp,cpp}
│   └── about_dialog.{hpp,cpp}
├── meson.build                 version 0.1.1
├── README.md
├── INSTALL.md
└── DEVELOPMENT.md
```

There is no `data/icons/` tree. The menu icon is `brand/icon-tile.svg`, installed as `earblaster.svg` under `hicolor/scalable/apps`.

### `Player`

Wrap one `GstElement* playbin`.

- `open(uri)`, `play()`, `pause()`, `stop()`, `seek(ns)`, `set_volume(0..1)`
- `set_eq_band` / `eq_band` on an `equalizer-10bands` bin hung off `audio-filter`
- Signals (sigc): `state_changed`, `position_changed`, `eos`, `error`, `tags`, `cover`
- Query duration/position on a 250 ms timeout only while playing
- Audio sink: default (Pulse or ALSA). `video-sink` is `fakesink` so a video file cannot open a picture
- File chooser and MIME filter: audio only (MP3, Ogg, FLAC, WAV, M4A at minimum)

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

Order: TagLib front cover → first embedded picture → sidecar names in the file’s directory → empty. `GST_TAG_IMAGE` is a last-chance path from `Player` if TagLib and sidecars miss.

### `Playlist`

`Gtk::ListStore` columns: title, artist, time, uri, duration_ns.

Two verbs, used everywhere (File menu, drag-drop). “Play” on the transport and Play menu means play/pause the current row only.

| Verb | List | Transport |
|---|---|---|
| **New** File / Folder / Playlist | replace | start the first new row immediately |
| **Add** File / Folder / Playlist | append | unchanged (keep playing, paused, or stopped) |

- **New File…** — multi-select; audio only; filename order; one file is a one-row list
- **New Folder…** — selected folder plus **one** level of subfolders (artist → albums). Audio only; filename order; play first. Skip hidden (`.`-prefixed) names.
- **Add File…** / **Add Folder…** — same scan rules, append
- Drag files or `.m3u` onto the pane = **Add**. Ignore a second `drag-data-received` for the same drop (0.1.1).
- Double-click a row = play that row (list unchanged)
- Stop resets the well; it does not clear the list. New/Add is what changes membership
- EOS / Next walk the list; after the last row, stop (unless Repeat)
- `GstDiscoverer` fills title/artist/time without opening the file in playbin

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

Shuffle / repeat flags live on `Playlist` and are persisted by `Settings`. Repeat in the UI is all-or-nothing (`Repeat::All` vs `Off`).

### `Settings` / `PrefsWindow`

`~/.config/earblaster/earblaster.ini` (Glib::KeyFile): window geometry, volume, ten EQ bands, shuffle, repeat, restore-window, music folder.

Edit → Preferences: music folder, restore window, shuffle, repeat. File choosers start in the saved music folder, else `~/Music` — never the source-tree `data/samples/` path.

### `Application`

`Gtk::Application` with a file lock plus uniqueness. A second launch focuses the existing window.

Uninstalled binary finds CSS and brand via `SOURCE_ROOT`. Installed binary uses `DATADIR`. `EARBLASTER_DATA` overrides both. When `APPDIR` is set (AppImage), `main.cpp` points GStreamer at the bundled plugin dir.

## 4. Skin (`data/skin/lcos/lcos.css`)

Stock GTK3 widgets. Clearlooks / Phenix keeps the window decorations.

- Client `#E6E6E1`
- TreeView selected row: steel blue, not Adwaita orange
- Scales and buttons: leave mostly to the desktop theme so it sits on XFCE
- `.earblaster-well` background `#0B1D38`

No custom title bar.

Dev machine (Debian Trixie + i3/Regolith or XFCE): compile with the packages in §6. Widget look can be forced with `GTK_THEME=Clearlooks-Phenix` if the engine is installed. Full title-bar fidelity needs xfwm4 + that theme (LCOS VM or an XFCE session).

## 5. Milestones

### M0 — Repo and window — **done 2026-09-08**

Meson project links gtkmm-3.0. `MainWindow`, menus, CSS load, `SealView` with a static ring + bolt and a static pill. About dialog uses `brand/lockup-pill.svg`. `.desktop` uses the icon tile.

Verified: Debian 13 aarch64, XFCE/X11.

### M1 — Spin — **done 2026-09-08**

Tick callback. A yellow bead runs the circumference once per second so motion is visible on a uniform ring. Bolt stays upright. Freeze on pause. Hide bead and reset angle on stop.

### M2 — Sound — **done 2026-09-08**

`Player` + transport. Seek and volume. Status bar `mm:ss / mm:ss`. Audio formats via playbin (MP3, Ogg, FLAC, WAV at minimum). No video. Embedded cover via `GST_TAG_IMAGE` after one bead lap. File menu later renamed Open → New.

### M3 — Playlist — **done 2026-09-08**

ListStore. File menu uses **New** (replace + start) vs **Add** (append, leave transport). New/Add File (multi-select), New/Add Folder (selected dir + one album level), M3U New/Add/Save. Drag-drop is Add. Double-click plays that row. Prev/Next, EOS → next, shuffle, repeat.

### M4 — Cover — **done 2026-09-08**

`CoverArt::load` into `SealView::set_cover`. TagLib first, then sidecar, then `GST_TAG_IMAGE`. After one bead lap, the image stretch-fills the well. Pill and mark hide. Stop restores the mark. No video widget.

### M5 — EQ + polish — **done 2026-09-08**

10-band dialog. Persist bands, window, volume, shuffle, repeat in `earblaster.ini`. Keyboard: Space play/pause, Left/Right seek ±5s, Delete remove. File lock + `GApplication` uniqueness.

Preferences (same evening, after the M5 commit): music folder, restore window, shuffle/repeat checkboxes. Choosers start in that folder.

### M6 — Package — **done 2026-09-08 / 0.1.1 on 2026-09-09**

In-tree `debian/` (native package, verified). Artifacts via `./scripts/release.sh`:

- **`.deb`** — preferred on LCOS / Devuan / Debian
- **source tarball** — `meson dist` (demo tracks under `data/samples/` stay git-only)
- **AppImage** — fallback; 0.1.1 copies GStreamer plugins into the AppDir so playbin loads. Still prefers host codecs if a format is missing. Needs `linuxdeploy` on `$PATH`.

Install layout: `/usr/bin/earblaster`, icons under `hicolor`, skin under `/usr/share/earblaster/skin/lcos`, brand SVGs under `/usr/share/earblaster/brand`. No daemon.

Tags: `v0.1.0` (first package + prefs), `v0.1.1` (duplicate-drop fix, AppImage plugin bundle).

**v1.0, as planned, is M0–M6.** The git tag is still 0.1.x until an LCOS install has been lived with.

### Later (not 0.1.x)

Spectrum ring outside the neon ring, playlist cover column, CUE sheets, MPRIS, gapless, CDDA, WinAmp skins.

## 6. Tooling (Devuan Excalibur / Debian Trixie)

```
sudo apt install \
  build-essential meson ninja-build pkg-config g++ \
  libgtkmm-3.0-dev \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly \
  gstreamer1.0-libav \
  libtag-dev
# gstreamer1.0-gtk3 (gtksink) is not used — audio only.
```

Package a `.deb` from this tree:

```
sudo apt install debhelper devscripts
./scripts/release.sh deb
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

`.clangd` points at `build/compile_commands.json` so gst headers resolve in nvim.

## 7. Packaging notes for LCOS

- Match `lcos-updates`: Meson + a small `debian/` directory. License is The Unlicense.
- Binary never runs as root
- Depends on GTK3, GStreamer good/ugly/libav, taglib
- Recommends Pulse or ALSA GStreamer plugins; do not vendor a window manager theme
- AppImage is a fallback (LCOS 0.3 already runs AppImages). Prefer `.deb`
- `debian/control` Vcs-* still names a private Gitea host. Public clone is GitHub. Fix those fields when touching packaging next.

## 8. Test matrix (0.1.x / planned 1.0)

- Open MP3 with APIC, MP3 without art, FLAC with picture, folder with only `folder.jpg`
- Video files are rejected (chooser filter); playbin video-sink is fakesink
- Pause freezes the bead; stop hides it
- Playlist EOS, shuffle, empty list, missing file
- Drag the same files once — each path appears once (0.1.1)
- Unplug Pulse → playbin should still find ALSA
- Second `earblaster` process focuses the first window
- XFCE + Clearlooks on XLibre: decorations and CSS do not fight
- AppImage finds playbin without host plugin packages for the formats bundled at build time

Already covered on Trixie: cold launch, About box, well resize, aarch64 compile (M0), native `.deb` build (M6).

## 9. After 0.1.1

M0–M6 are closed. Do not open an M7 in this document.

Small packaging nits, not a milestone:

- Point `debian/control` Homepage / Vcs-* at GitHub
- Live-test the `.deb` on LCOS 0.3, then consider tag `v1.0.0`

Product work after that is the parked list in §5, or the next LCOS-only app (Cardfile) — not more EarBlaster scope unless a bug shows up on the target desktop.
