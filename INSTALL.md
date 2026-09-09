# Installing EarBlaster

Four ways to get a binary, in the order LCOS cares about:

| Artifact | Who it is for |
|---|---|
| **`.deb`** | LCOS, Devuan Excalibur, Debian Trixie. Preferred. |
| **Source tarball** | Distro packagers and `meson setup && ninja install`. |
| **AppImage** | Fallback when you cannot install packages. Needs host GStreamer codecs. |
| **Git build** | Developers. See below. |

Version comes from `meson.build` (currently `0.1.3`).

## Runtime needs (all installs except a fully bundled AppImage)

- GTK 3 / gtkmm-3.0
- GStreamer 1.0 + **good**, **ugly**, and **libav** plugins (MP3, etc.)
- TagLib 2.x
- PulseAudio or ALSA (GStreamer default sink)

On Debian / Devuan / LCOS:

```
sudo apt install \
  libgtkmm-3.0-1t64 \
  libgstreamer1.0-0 libgstreamer-plugins-base1.0-0 \
  gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly \
  gstreamer1.0-libav \
  libtag2
```

(Package names on older Debian may be `libgtkmm-3.0-1v5` / `libtag1v5`.)

## 1. Debian package (preferred)

From a release `.deb`:

```
sudo apt install ./dist/earblaster_0.1.3-1_amd64.deb
```

Or, from this tree:

```
./scripts/release.sh deb
sudo apt install ./dist/earblaster_0.1.3-1_amd64.deb
```

That installs:

- `/usr/bin/earblaster`
- `/usr/share/applications/earblaster.desktop`
- `/usr/share/icons/hicolor/scalable/apps/earblaster.svg`
- `/usr/share/earblaster/skin/lcos/lcos.css`
- `/usr/share/earblaster/brand/*.svg`

Launch from the menu or `earblaster`. Config is `~/.config/earblaster/earblaster.ini`.

Uninstall: `sudo apt remove earblaster`.

## 2. Source tarball

`meson dist` produces `build/meson-dist/earblaster-VERSION.tar.xz` (demo audio under `data/samples/` is git-only, not in the tarball).

```
tar -xf earblaster-0.1.3.tar.xz
cd earblaster-0.1.3
sudo apt install build-essential meson ninja-build pkg-config \
  libgtkmm-3.0-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  libtag-dev
meson setup build --prefix=/usr
meson compile -C build
sudo meson install -C build
```

`./scripts/release.sh tarball` runs `meson dist` for you.

## 3. AppImage (fallback)

LCOS 0.3 already runs AppImages. The image **bundles GStreamer from the build host** (libgstreamer + plugins, including playbin). It does **not** load the host's plugins: mixing Debian's libgstreamer with Arch's `libgstplayback.so` fails (`undefined symbol: gst_log_context_get_category`). Codecs are whatever was on the build machine.

```
./scripts/release.sh appimage
```

Requires `linuxdeploy` on `$PATH` (see <https://github.com/linuxdeploy/linuxdeploy>). Output: `dist/EarBlaster-VERSION-x86_64.AppImage`.

Mark executable and run. The AppImage runtime sets `APPDIR` to the mounted image; EarBlaster then uses **only** `$APPDIR/usr/lib/gstreamer-1.0` and a private registry (`~/.cache/gstreamer-1.0/earblaster-appimage.bin`).

```
chmod +x EarBlaster-*.AppImage
./EarBlaster-*.AppImage
```

If you extract the image, set `APPDIR` yourself or host plugins will mix with the bundled libgstreamer:

```
./EarBlaster-*.AppImage --appimage-extract
export APPDIR="$PWD/squashfs-root"
"$APPDIR/AppRun"
```

Leave `APPDIR` unset for `.deb` and `meson install` builds. If a format is missing, that codec was not on the build host; use the `.deb` on Debian/LCOS or a native build on Arch.

## 4. Developer build (no install)

```
meson setup build
meson compile -C build
./build/earblaster
```

The binary finds CSS and brand files via `SOURCE_ROOT` in the build tree. `EARBLASTER_DATA` overrides that.

## One command for every artifact

```
./scripts/release.sh all
```

Writes tarball, `.deb`, and AppImage (if `linuxdeploy` is present) under `dist/` and the parent directory for Debian artifacts.

## What this project will not ship

- Video codecs or a video sink (VLC is the LCOS video player)
- A systemd unit
- Vendored Clearlooks / xfwm themes (Recommends the desktop theme)
