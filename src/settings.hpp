/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <string>

namespace earblaster {

struct Settings {
  static constexpr int kEqBands = 10;

  /* Any coordinate is valid (x or y < 0 on a monitor left of / above the
   * primary), so whether a position was saved is its own flag. */
  bool has_position = false;
  int window_x = 0;
  int window_y = 0;
  int window_w = 800;
  int window_h = 534;
  double volume = 0.8;
  bool shuffle = false;
  bool repeat = false;
  bool restore_window = true;
  std::string music_dir;
  double eq[kEqBands] = {};

  void load();
  void save() const;
};

}  // namespace earblaster
