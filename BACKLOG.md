# EarBlaster backlog

Current release: **v1.1.11**. Last updated: 2026-10-02.

Windows Media Player 7 (audio). Binary `earblaster`. Suite catalog: `lcos-projects/PRODUCT-BACKLOG.md`. Plan: [DEVELOPMENT.md](DEVELOPMENT.md). How to land work: [DEVELOPMENT.md](DEVELOPMENT.md#process) — `feature/` / `fix/` branches, PRs to `master`, lint gate, semver on shipped PRs.

## High Priority

None.

## Low Priority

- Spectrum ring outside the neon ring
- MPRIS
- Gapless playback
- CDDA
- WinAmp skins (`.wsz`)

## Out of Scope

- Video. VLC stays the LCOS generic player. `video-sink` is `fakesink`.
- Network in the default build (no streaming product, no online account)
- systemd / user-bus daemon required to launch
- Custom title bar. `GTK_THEME` in the environment still wins; else Clearlooks-Phenix, then Clearlooks, then Adwaita:light (process only)
- Bryan Lunduke’s official LCOS seal
- Qmmp / Audacious rebrand
- Mixing host GStreamer plugins with the bundled AppImage library (0.1.2 closed this; do not regress)
- Tag editing in this window. That is a separate guest: media library manager (`lcos-projects/MEDIA-LIBRARY.md`). EarBlaster plays.

## Shipped

**v1.1.11** — A Sync folder copy that failed part-way can be resumed with a second Transfer.

**v1.1.10** — Sync no longer recurses forever through a symlink to a folder or its ancestor.

**v1.1.9** — A window on a monitor left of or above the primary is restored there.

**v1.1.8** — A cue sheet whose FILE line is a Windows absolute path finds the audio next to the sheet.

**v1.1.7** — Save Playlist keeps CUE chapter ranges, titles, and artists.

**v1.1.6** — Opening a cue sheet together with its audio file adds only the chapter rows.

**v1.1.5** — Shuffle Previous returns to the same track after rows are deleted or dragged.

**v1.1.4** — Removing the playing row continues with the row that followed it.

**v1.1.3** — Stop, then Play, seeks back to the same CUE chapter.

**v1.1.2** — A cue sheet that starts with a UTF-8 BOM still loads its tracks.

**v1.1.1** — Headless meson test suite, and known defects recorded in BUG-BACKLOG.md.

**v1.1.0** — Playlist cover column (32px TagLib/sidecar thumbs). CUE sheets expand to TITLE/PERFORMER tracks; playbin seeks INDEX 01 ranges.

**v0.1.0 (M0–M6)** — gtkmm-3 window, spinning ring + bead, GStreamer `playbin` audio, New vs Add playlist (files, one album folder, M3U, drop), cover art (TagLib → sidecar → `GST_TAG_IMAGE`), 10-band EQ, prefs, keyboard, single-instance, `.deb` / tarball / AppImage. Config: `~/.config/earblaster/earblaster.ini`.

**v0.1.0-2** — Preferences: music folder, restore window, shuffle, repeat. Choosers start in the saved music folder.

**v0.1.1** — Drag-and-drop no longer adds each file twice. AppImage bundles GStreamer plugins.

**v0.1.2** — AppImage uses only bundled plugins and a private registry.

**v0.1.3** — Navy SVG transport buttons (replaces ASCII). Lived with on LCOS 0.3, 0.4, and 0.5.

**v0.2.0** — XFCE/Thunar Open with (MimeType + `Exec=%F`). Cold start reads argv as **New**. Second instance uses `$XDG_RUNTIME_DIR/earblaster.sock` (no D-Bus, no systemd). Video types stay with VLC.

**v0.2.1** — Stale album M3Us play: missing `.ogg` entries resolve to existing files (extension swap or unique leading track number); GstDiscoverer does not open the URI playbin is using.

**v0.2.2** — GTK theme fallback: Clearlooks-Phenix, then Clearlooks, then Adwaita:light.

**v0.2.3** — debian/control Homepage and Vcs-* point at the public GitHub clone.

**v1.0.0** — Offline device sync. Menubar **Sync** (far right) opens a Midnight Commander split: left = prefs music folder; right = already-mounted USB block or MTP. Tick either side; Transfer copies via Gio (`file://` and `mtp://`). Identical names are skipped. No transcode, no D-Bus to launch. Plan: [SYNC.md](SYNC.md).
