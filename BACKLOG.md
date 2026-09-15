# EarBlaster backlog

Current release: **v0.1.3**. Last updated: 2026-09-15.

Windows Media Player 7 (audio). Binary `earblaster`. Suite catalog: `lcos-projects/PRODUCT-BACKLOG.md`. Plan: [DEVELOPMENT.md](DEVELOPMENT.md). How to land work: [DEVELOPMENT.md](DEVELOPMENT.md#process) — `feature/` / `fix/` branches, PRs to `master`, lint gate, semver on shipped PRs.

## High Priority

- **XFCE “Open with” / default audio handler.** Right-click an MP3 or Ogg in Thunar and Open with EarBlaster; optionally make it the default for those types on LCOS. Today `earblaster.desktop` has no `MimeType=` and `Exec=` takes no files; argv is ignored. Need: `MimeType` for the audio types we already play (at least `audio/mpeg`, `audio/ogg`, plus FLAC / WAV / M4A); `Exec=earblaster %F`; cold start reads those paths from argv and treats them as **New** (replace + start). **No D-Bus, no systemd, no `APPLICATION_HANDLES_OPEN`.** Single-instance stays the flock. If already running, the second process writes the paths to a Unix socket next to the lock file (`$XDG_RUNTIME_DIR/earblaster.sock`) and exits; the primary watches that socket and does **New**. Do not steal video types (VLC stays the generic player). Then consider tag `v1.0.0`.
- Point `debian/control` Homepage / Vcs-* at the public clone when packaging is touched next. Origin stays Gitea; GitHub is the mirror.

## Low Priority

- Spectrum ring outside the neon ring
- Playlist cover column
- CUE sheets
- MPRIS
- Gapless playback
- CDDA
- WinAmp skins (`.wsz`)

## Out of Scope

- Video. VLC stays the LCOS generic player. `video-sink` is `fakesink`.
- Network in the default build (no streaming product, no online account)
- systemd / user-bus daemon required to launch
- Custom title bar; do not override `GTK_THEME` (prefer-light only)
- Bryan Lunduke’s official LCOS seal
- Qmmp / Audacious rebrand
- Mixing host GStreamer plugins with the bundled AppImage library (0.1.2 closed this; do not regress)

## Shipped

**v0.1.0 (M0–M6)** — gtkmm-3 window, spinning ring + bead, GStreamer `playbin` audio, New vs Add playlist (files, one album folder, M3U, drop), cover art (TagLib → sidecar → `GST_TAG_IMAGE`), 10-band EQ, prefs, keyboard, single-instance, `.deb` / tarball / AppImage. Config: `~/.config/earblaster/earblaster.ini`.

**v0.1.0-2** — Preferences: music folder, restore window, shuffle, repeat. Choosers start in the saved music folder.

**v0.1.1** — Drag-and-drop no longer adds each file twice. AppImage bundles GStreamer plugins.

**v0.1.2** — AppImage uses only bundled plugins and a private registry.

**v0.1.3** — Navy SVG transport buttons (replaces ASCII). Lived with on LCOS 0.3, 0.4, and 0.5. Tag `v1.0.0` waits on XFCE Open-with (High Priority).
