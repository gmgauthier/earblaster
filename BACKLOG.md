# EarBlaster backlog

Current release: **v0.2.2**. Last updated: 2026-09-16.

Windows Media Player 7 (audio). Binary `earblaster`. Suite catalog: `lcos-projects/PRODUCT-BACKLOG.md`. Plan: [DEVELOPMENT.md](DEVELOPMENT.md). How to land work: [DEVELOPMENT.md](DEVELOPMENT.md#process) — `feature/` / `fix/` branches, PRs to `master`, lint gate, semver on shipped PRs.

## High Priority

- Point `debian/control` Homepage / Vcs-* at the public clone when packaging is touched next. Origin stays Gitea; GitHub is the mirror.
- After Open with has been lived with on LCOS 0.5, consider tag `v1.0.0`.

## Low Priority

- Spectrum ring outside the neon ring
- Playlist cover column
- CUE sheets
- MPRIS
- Gapless playback
- CDDA
- WinAmp skins (`.wsz`)
- **In-place tag edit** — right-click a row → Edit → ten TagLib fields, Save writes and closes. Stop (or refuse) if that file is playing, especially on SMB. Plan: [TAG-EDIT.md](TAG-EDIT.md). MINOR.

## Out of Scope

- Video. VLC stays the LCOS generic player. `video-sink` is `fakesink`.
- Network in the default build (no streaming product, no online account)
- systemd / user-bus daemon required to launch
- Custom title bar. `GTK_THEME` in the environment still wins; else Clearlooks-Phenix, then Clearlooks, then Adwaita:light (process only)
- Bryan Lunduke’s official LCOS seal
- Qmmp / Audacious rebrand
- Mixing host GStreamer plugins with the bundled AppImage library (0.1.2 closed this; do not regress)

## Shipped

**v0.1.0 (M0–M6)** — gtkmm-3 window, spinning ring + bead, GStreamer `playbin` audio, New vs Add playlist (files, one album folder, M3U, drop), cover art (TagLib → sidecar → `GST_TAG_IMAGE`), 10-band EQ, prefs, keyboard, single-instance, `.deb` / tarball / AppImage. Config: `~/.config/earblaster/earblaster.ini`.

**v0.1.0-2** — Preferences: music folder, restore window, shuffle, repeat. Choosers start in the saved music folder.

**v0.1.1** — Drag-and-drop no longer adds each file twice. AppImage bundles GStreamer plugins.

**v0.1.2** — AppImage uses only bundled plugins and a private registry.

**v0.1.3** — Navy SVG transport buttons (replaces ASCII). Lived with on LCOS 0.3, 0.4, and 0.5.

**v0.2.0** — XFCE/Thunar Open with (MimeType + `Exec=%F`). Cold start reads argv as **New**. Second instance uses `$XDG_RUNTIME_DIR/earblaster.sock` (no D-Bus, no systemd). Video types stay with VLC.

**v0.2.1** — Stale album M3Us play: missing `.ogg` entries resolve to existing files (extension swap or unique leading track number); GstDiscoverer does not open the URI playbin is using.
