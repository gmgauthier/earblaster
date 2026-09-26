/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <glibmm/ustring.h>

#include <string>
#include <vector>

namespace earblaster {

struct CueTrack {
  Glib::ustring title;
  Glib::ustring performer;
  gint64 start_ns = -1;
};

struct CueFile {
  std::string audio_path;
  Glib::ustring performer;
  std::vector<CueTrack> tracks;
};

/* Parse a .cue next to its audio file. Paths in FILE are relative to the
 * sheet. INDEX 01 is MM:SS:FF (75 frames/s). Multiple FILE blocks allowed. */
std::vector<CueFile> parse_cue_sheet(const std::string& cue_path);

}  // namespace earblaster
