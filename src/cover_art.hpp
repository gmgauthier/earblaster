/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <gdkmm/pixbuf.h>

#include <string>

namespace earblaster {

/* Embedded TagLib picture first, then sidecar folder.jpg / cover.jpg / etc. */
Glib::RefPtr<Gdk::Pixbuf> load_cover(const std::string& path_or_uri);

}  // namespace earblaster
