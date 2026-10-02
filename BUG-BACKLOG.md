# Bug backlog

Reviewed 2026-10-01 against the 1.1.0 sources.

`meson test` runs `tests/test_cue.cpp` (`cue`), `tests/test_player.cpp` (`player`), and `tests/test_playlist.cpp` (`playlist`). `cue` checks quoted relative `FILE` lines, `INDEX 00` dropped, `INDEX 01` frame math within 1 ms, absolute paths, and a UTF-8 BOM in front of a sheet. `player` checks that Stop arms the chapter seek again, that Pause does not, and that a file with no chapter range stays unarmed. The player test sets `EARBLASTER_AUDIO_SINK=fakesink`. Nothing in `src/` shells out.

## Open

### Shuffle back-history stores raw indexes

- Severity: incorrect
- Confidence: high
- Where: `src/playlist.cpp:669`, `src/playlist.cpp:694`, `src/playlist.cpp:746`
- Trigger: Shuffle on, press Next a few times, then delete a row above a remembered index or drag a row, then press Previous.
- Outcome: `history_` stores integer row indexes. `remove_paths` does not edit it. Previous jumps to whatever row now sits at the stale index. Indexes past the new size are dropped and sequential Previous runs instead.

### Opening a CUE plus its audio file adds the full file again

- Severity: incorrect
- Confidence: high
- Where: `src/main_window.cpp:446`, `src/playlist.cpp:533`
- Trigger: `earblaster album.cue album.flac`, or File → New File selecting the sheet and the audio together.
- Outcome: `.cue` paths go through `add_cue` and everything else through `add_files`. The "already claimed by a cue" skip lives only inside `add_files`. The playlist gets one row per chapter and an extra row of the same file from 0 with no stop. That extra row plays the whole album.

### Save Playlist throws away CUE ranges

- Severity: data-loss
- Confidence: high
- Where: `src/playlist.cpp:654`
- Trigger: Load a `.cue` with several `INDEX 01` tracks, File → Save Playlist, open that `.m3u`.
- Outcome: Each row is written as a path. `start_ns` and `stop_ns` are not written. Reload plays the full file once per chapter, from the start, back to back. Titles and chapter bounds are gone.

### Windows absolute FILE paths in a CUE never resolve

- Severity: incorrect
- Confidence: high
- Where: `src/cue_sheet.cpp:127`
- Trigger: A sheet next to `album.flac` whose line is `FILE "D:\Music\Album\album.flac" WAVE`.
- Outcome: Backslashes become slashes, then a drive letter (`name[1] == ':'`) is stored as absolute with no basename fallback. The path is `D:/Music/Album/album.flac`. On Linux that file is not found and `add_cue` adds nothing. Relative names and backslash-relative names are joined to the sheet directory.

### Restored window position ignores negative coordinates

- Severity: incorrect
- Confidence: high
- Where: `src/main_window.cpp:120`
- Trigger: Move the window onto a monitor whose origin is left of or above the primary (`x` or `y` < 0), quit, start again with "Restore window position" on.
- Outcome: `persist()` saves `get_position` as-is. Restore moves only when both coordinates are `>= 0`. The unset sentinel is also `-1`. Size is restored; position is not. A left-hand panel in a side-by-side layout is a normal way to get `x < 0`.

### Sync follows a directory symlink and recurses forever

- Severity: crash
- Confidence: high
- Where: `src/sync_window.cpp:59`, `src/sync_window.cpp:83`
- Trigger: Transfer a folder that contains a symlink to itself or to an ancestor.
- Outcome: `dir_type` is true for `FILE_TYPE_DIRECTORY`. Gio's enumerate follows links, so a symlink to a directory is reported as a directory. `copy_tree` has no symlink check and no visited set. It walks the same tree until the stack overflows.

### A failed folder copy cannot be resumed

- Severity: data-loss
- Confidence: high
- Where: `src/sync_window.cpp:81`, `src/sync_window.cpp:96`
- Trigger: Copy a folder of several files. One file fails after the destination directory has been created. Tick the same folder and Transfer again.
- Outcome: `query_exists` on the destination name returns false from `copy_tree`, which the caller reports as "already exists". The child `copy_tree` result is ignored, and a throw from `src->copy` aborts the rest of that folder. The retry skips the whole tree. Those files stay off the device.

### Cancelling a directory transfer can report success

- Severity: incorrect
- Confidence: high
- Where: `src/sync_window.cpp:89`
- Trigger: Tick one folder, Transfer, and close the Sync window while children are still copying, and that folder is the only job.
- Outcome: Inside the child loop, cancel returns `true`. The caller counts that as a copied item, and `cancelled` is set only at the start of the next top-level item. The status line says `Copied 1 item(s).` The destination folder is incomplete.

### Second-instance socket commits a truncated path list

- Severity: incorrect
- Confidence: medium
- Where: `src/application.cpp:160`, `src/application.cpp:180`, `src/main_window.cpp:442`
- Trigger: A second process hands the primary more than about 1 MB of newline-separated paths.
- Outcome: The accept handler stops reading at 1 MB and still calls `handle_open_payload`. The sender returns success after a short `write`. `open_paths` clears the playlist first, so the primary replaces the current playlist with a cut-off list. The last line can be a partial path, and nothing retries.

## Closed

### Removing the playing row restarts at row 0

- Severity: incorrect
- Confidence: high
- Where: `src/main_window.cpp:557`, `src/playlist.cpp:677`, `src/playlist.cpp:759`
- Trigger: Play track 3 of 5 (not shuffled). Delete that row.
- Outcome: `remove_paths` clears the current row reference. `next()` treats a missing current index as 0, so playback starts at the first remaining row. With shuffle and more than one row left, it picks a random row instead of the successor.
- Fixed in v1.1.4: Removing the playing row remembers the row that followed it. Next (and the auto-advance after the delete) plays that row, with or without shuffle. Removing the last row stops, or wraps to row 0 under Repeat.

### Stop then Play drops the CUE chapter range

- Severity: incorrect
- Confidence: high
- Where: `src/player.cpp` `play`, `stop`
- Trigger: Play a CUE track whose `INDEX 01` is not `00:00:00`. Press Stop, then Play.
- Outcome: `stop()` sets playbin to `NULL` and does not clear `uri_`. `loaded()` is `!uri_.empty()`, so Play calls `player_.play()`, which only sets `PLAYING`. `pending_clip_seek_` is set in `open()` and is already false. Audio starts at the beginning of the file. `clip_stop_` is still the chapter end, so playback runs from 0 until that absolute time and then skips. Chapter 1 (start 0) happens to sound right.
- Fixed in v1.1.3: Stop arms the chapter seek again, and Play from Stopped seeks to that range. Pause leaves the seek where it is.

### A UTF-8 BOM makes the cue sheet empty

- Severity: incorrect
- Confidence: high
- Where: `src/cue_sheet.cpp` `parse_cue_sheet`
- Trigger: A `.cue` whose first bytes are `EF BB BF`, then a valid `FILE` / `TRACK` / `INDEX 01`.
- Outcome: `ifstream::peek()` returns `int` `0xEF`. `'\xEF'` is a signed char on this ABI, so the comparison is false and the BOM is left in the first line. `FILE` is not recognized. `parse_cue_sheet` returns no tracks, and the sheet is ignored.
- Fixed in v1.1.2: The leading UTF-8 BOM is recognized as the integer `0xEF` and skipped. A sheet that only starts with `EF BB` and a different third byte is left unchanged.
