# In-place tag edit (parked)

Last updated: 2026-09-16. Status: **Low Priority**. MINOR when shipped.

Right-click a playlist row → **Edit** → a panel with the ten common text fields, populated from the file, **Save** writes tags and closes. Not a cover-art editor. Not v1; WMP 7 did not tag from the playlist.

## UI

- Right-click on `playlist_view_`: select the row under the pointer, `Gtk::Menu` with **Edit**.
- Dialog in the same style as Preferences / EQ: decorated gtkmm-3 window, no custom title bar, no `GTK_THEME` override.
- Ten `Gtk::Entry`s. Empty means “no value in the file.”
- **Save** writes and `hide()`s. Window chrome × / Escape = close without write.
- After a successful save, update that row’s title/artist so the list matches the file.
- Save failure: status line, keep the dialog open.

## Fields (TagLib `PropertyMap`)

| Field | TagLib key |
|---|---|
| Title | `TITLE` |
| Artist | `ARTIST` |
| Album | `ALBUM` |
| Album artist | `ALBUMARTIST` |
| Track | `TRACKNUMBER` |
| Disc | `DISCNUMBER` |
| Year | `DATE` / `YEAR` |
| Genre | `GENRE` |
| Comment | `COMMENT` |
| Composer | `COMPOSER` |

ID3v2 (MP3), Vorbis comments (Ogg/FLAC), MP4 (M4A) — the formats we already play. Cover art is a binary `PICTURE` and stays out of this panel.

## Write

`TagLib::FileRef` is already linked (`cover_art.cpp`). Read with `properties()`, write with `setProperties()` + `save()`. Empty field on save = remove that key. Do not invent a tag format; do not rewrite the whole block if TagLib can merge.

## Playing file (the trap)

playbin has the URI open. Local disk often still saves; **CIFS/SMB** (`/data/Audio/...`) often fails or stalls. If the row is the current track: **stop, save, reopen** (or refuse Save until Stop). Do not write under a live playbin.

Read-only / missing file: same as M3U — do not pretend the write worked.

## Out of scope

- Cover art picker
- Multi-select “edit all”
- MusicBrainz / lookup
- Renaming the file from title
- D-Bus, extra Debian deps (`libtag-dev` is already there)

## Shape

One `feature/tag-edit` PR: dialog class + tree `button_press` + a small TagLib read/write helper. Comparable to Preferences, plus the stop-before-save rule. No meson dependency change.
