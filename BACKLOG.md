# EarBlaster backlog

Current release: **v0.1.3**. Last updated: 2026-09-15.

Windows Media Player 7 (audio). Binary `earblaster`. Suite catalog: `lcos-projects/PRODUCT-BACKLOG.md`. Plan: [DEVELOPMENT.md](DEVELOPMENT.md). How to land work: [DEVELOPMENT.md](DEVELOPMENT.md#process) — `feature/` / `fix/` branches, PRs to `master`, lint gate, semver on shipped PRs.

## High Priority

- Live-test the `.deb` on LCOS 0.3, then consider tag `v1.0.0`. M0–M6 already match the planned v1 feature set; the git tag is still 0.1.x until the package has been lived with.
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

**v0.1.3** — Navy SVG transport buttons (replaces ASCII).
