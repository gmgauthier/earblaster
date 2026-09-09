#!/bin/sh
# Build release artifacts: tarball, .deb, optional AppImage.
# Usage: ./scripts/release.sh [tarball|deb|appimage|all]

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

VERSION=$(sed -n "s/^  version: '\\(.*\\)',/\\1/p" meson.build | head -1)
DISTDIR="${ROOT}/dist"
JOB=${1:-all}

mkdir -p "$DISTDIR"

need_build() {
  if [ ! -f "${ROOT}/build/build.ninja" ]; then
    meson setup "${ROOT}/build" "$ROOT"
  fi
}

do_tarball() {
  need_build
  meson dist -C "${ROOT}/build" --no-tests --allow-dirty
  # meson dist writes build/meson-dist/earblaster-VERSION.tar.xz
  src="${ROOT}/build/meson-dist/earblaster-${VERSION}.tar.xz"
  if [ -f "$src" ]; then
    cp -f "$src" "$DISTDIR/"
    echo "tarball: ${DISTDIR}/earblaster-${VERSION}.tar.xz"
  else
    echo "meson dist did not produce earblaster-${VERSION}.tar.xz" >&2
    ls -la "${ROOT}/build/meson-dist" >&2 || true
    exit 1
  fi
}

do_deb() {
  dpkg-buildpackage -us -uc -b --no-sign
  mkdir -p "$DISTDIR"
  for f in "${ROOT}/../earblaster_${VERSION}"-*.deb \
           "${ROOT}/../earblaster-dbgsym_${VERSION}"-*.deb; do
    [ -e "$f" ] || continue
    mv -f "$f" "$DISTDIR/"
    echo "deb: $DISTDIR/$(basename "$f")"
  done
  # dpkg-buildpackage also drops .buildinfo/.changes next to the repo
  for f in "${ROOT}/../earblaster_${VERSION}"-*.buildinfo \
           "${ROOT}/../earblaster_${VERSION}"-*.changes; do
    [ -e "$f" ] || continue
    mv -f "$f" "$DISTDIR/"
  done
}

do_appimage() {
  if ! command -v linuxdeploy >/dev/null 2>&1; then
    echo "linuxdeploy not on PATH; skip AppImage." >&2
    echo "See INSTALL.md §3." >&2
    return 0
  fi
  APPDIR="${ROOT}/build/AppDir"
  rm -rf "$APPDIR"
  meson setup "${ROOT}/build-appimage" "$ROOT" --prefix=/usr
  meson compile -C "${ROOT}/build-appimage"
  DESTDIR="$APPDIR" meson install -C "${ROOT}/build-appimage"
  # playbin is a GStreamer *plugin*, not a link dependency. Copy plugins
  # (and the scanner) into the AppDir so the image can actually play.
  GST_PLUGINS=$(pkg-config --variable=pluginsdir gstreamer-1.0)
  if [ -n "$GST_PLUGINS" ] && [ -d "$GST_PLUGINS" ]; then
    mkdir -p "${APPDIR}/usr/lib/gstreamer-1.0"
    cp -a "${GST_PLUGINS}"/*.so "${APPDIR}/usr/lib/gstreamer-1.0/" 2>/dev/null || true
  fi
  SCANNER=$(ls /usr/lib/*/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner 2>/dev/null | head -1)
  if [ -n "$SCANNER" ] && [ -x "$SCANNER" ]; then
    mkdir -p "${APPDIR}/usr/lib/gstreamer1.0/gstreamer-1.0"
    cp -a "$SCANNER" "${APPDIR}/usr/lib/gstreamer1.0/gstreamer-1.0/"
  fi
  export LINUXDEPLOY_OUTPUT_VERSION="$VERSION"
  export APPIMAGE_EXTRACT_AND_RUN=1
  PLUGIN_ARGS=""
  if command -v linuxdeploy-plugin-gtk >/dev/null 2>&1 || \
     [ -x "${ROOT}/scripts/linuxdeploy-plugin-gtk.sh" ]; then
    PLUGIN_ARGS="--plugin gtk"
    if [ -x "${ROOT}/scripts/linuxdeploy-plugin-gtk.sh" ]; then
      export PATH="${ROOT}/scripts:${PATH}"
    fi
  fi
  # GStreamer plugins stay on the host; see INSTALL.md.
  # shellcheck disable=SC2086
  linuxdeploy --appdir "$APPDIR" \
    --executable "${APPDIR}/usr/bin/earblaster" \
    --desktop-file "${APPDIR}/usr/share/applications/earblaster.desktop" \
    --icon-file "${APPDIR}/usr/share/icons/hicolor/scalable/apps/earblaster.svg" \
    $PLUGIN_ARGS \
    --output appimage
  mkdir -p "$DISTDIR"
  # appimagetool writes next to the AppDir / cwd
  for f in "${ROOT}/EarBlaster-${VERSION}"-*.AppImage \
           "${ROOT}/build/EarBlaster-${VERSION}"-*.AppImage \
           "${ROOT}"/*.AppImage; do
    [ -e "$f" ] || continue
    mv -f "$f" "$DISTDIR/"
  done
  echo "appimage: $(ls -1 "$DISTDIR"/*.AppImage 2>/dev/null | tail -1)"
}

case "$JOB" in
  tarball) do_tarball ;;
  deb)     do_deb ;;
  appimage) do_appimage ;;
  all)
    do_tarball
    do_deb
    do_appimage
    ;;
  *)
    echo "Usage: $0 [tarball|deb|appimage|all]" >&2
    exit 2
    ;;
esac
