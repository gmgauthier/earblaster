# Offline device sync (High)

Last updated: 2026-09-17. Status: **Shipped in v1.0.0**.

WMP “Copy to CD or Device” / iTunes-to-iPod / Palm HotSync as a **file copy**, not a cloud account. Midnight Commander: two vertical panes, tick files, Transfer copies to the other side.

## UI

- Toolbar / menubar **Sync**, far right (same slot as Dispatch `[MAIL]` / `[FEED]`). Mock: [brand/mockup-sync.png](brand/mockup-sync.png).
- Pop-out window, two panes:
  - **Left** — local filesystem. Default start: prefs **music folder** (already in EarBlaster).
  - **Right** — the attached device. User picks the music directory on that device (e.g. `/Music`, `/Internal storage/Music`).
- Tick boxes on either side. **Transfer** copies ticked items to the other pane’s current directory (left→right or right→left depending on which side is ticked). Use Gio copy (not raw `cp`) so MTP works. Progress in the status line. Close does not imply Transfer.

## Devices

Anything the session already mounted. Do not become udisks/gvfsd. If the right pane is empty, say so.

**USB mass-storage (block)** — example on this machine: AGPTek **SURFANS-F20** (`c502:0023`) as `Linux File-CD Gadget`, `/dev/sda1` **exFAT**, udisks mount `/media/$USER/<UUID>` (here `/media/gmgauthier/3864-3134`). Music lives at the **volume root** (genre folders: Classical, Rock, …). POSIX paths work (`file://`).

**MTP** — example: Samsung Galaxy (`04e8:6860`) as gio volume **SAMSUNG Android**, display name **Ephialtes**, URI `mtp://SAMSUNG_SAMSUNG_Android_…/`. Volumes: `Internal storage`, `SD card`. Device **root is not writable**; `Internal storage` is. There is often **no FUSE path** under `/run/user/$UID/gvfs` — do not `std::filesystem` the right pane. Use **Gio::File** for list/copy so `file://` and `mtp://` share one code path.

Do **not** offer host disks (nvme, `/`, `/data`) as the “device.” Filter: USB `TRAN=usb` block mounts, plus gio `mtp://` / `mtpfs` volumes.

Do **not** require D-Bus to *launch* EarBlaster. Optional later: libmtp if nothing is mounted.

No network, no Apple ID, no “library database” on the device. Copy files. The device plays whatever it can (MP3/Ogg/etc.). Default left pane: prefs `music_dir` (here `/data/Audio/Music`).

## Out of scope (this slice)

- Transcoding on copy (e.g. FLAC→MP3 for a cheap player)
- Two-way “smart playlist” sync / iTunes library XML
- CD burning (that is CDDA / another slice)
- A second guest app. This lives in EarBlaster.
