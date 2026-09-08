# Installing EarBlaster

Four ways to get a binary, in the order LCOS cares about:

| Artifact | Who it is for |
|---|---|
| **`.deb`** | LCOS, Devuan Excalibur, Debian Trixie. Preferred. |
| **Source tarball** | Distro packagers and `meson setup && ninja install`. |
| **AppImage** | Fallback when you cannot install packages. Needs host GStreamer codecs. |
| **Git build** | Developers. See below. |

Version comes from `meson.build` (currently `0.1.0`).

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
sudo apt install ./earblaster_0.1.0-1_amd64.deb
```

Or, from this tree:

```
./scripts/release.sh deb
sudo apt install ../earblaster_0.1.0-1_amd64.deb
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

`meson dist` produces `build/meson-dist/earblaster-VERSION.tar.xz`.

```
tar -xf earblaster-0.1.0.tar.xz
cd earblaster-0.1.0
sudo apt install build-essential meson ninja-build pkg-config \
  libgtkmm-3.0-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  libtag-dev
meson setup build --prefix=/usr
meson compile -C build
sudo meson install -C build
```

`./scripts/release.sh tarball` runs `meson dist` for you.

## 3. AppImage (fallback)

LCOS 0.3 already runs AppImages. The AppImage still **uses the host GStreamer plugins** so MP3/etc. stay with the distro. It is not a fully self-contained codec bundle (that would ship ffmpeg/libav and become huge).

```
./scripts/release.sh appimage
```

Requires `linuxdeploy` on `$PATH` (see <https://github.com/linuxdeploy/linuxdeploy>). Output: `dist/EarBlaster-VERSION-x86_64.AppImage`.

Mark executable and run:

```
chmod +x EarBlaster-*.AppImage
./EarBlaster-*.AppImage
```

If playback has no codecs, install the GStreamer plugin packages above on the host.

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
