/* SPDX-License-Identifier: Unlicense */

#pragma once

namespace earblaster {

struct Settings {
  static constexpr int kEqBands = 10;

  int window_x = -1;
  int window_y = -1;
  int window_w = 800;
  int window_h = 534;
  double volume = 0.8;
  bool shuffle = false;
  bool repeat = false;
  double eq[kEqBands] = {};

  void load();
  void save() const;
};

}  // namespace earblaster
