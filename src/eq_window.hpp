/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "player.hpp"
#include "settings.hpp"

#include <gtkmm.h>

#include <array>

namespace earblaster {

class EqWindow : public Gtk::Window {
 public:
  EqWindow(Player& player, Settings& settings);

 private:
  void on_band(int i);
  void on_reset();

  Player& player_;
  Settings& settings_;
  Gtk::Box root_{Gtk::ORIENTATION_VERTICAL, 6};
  Gtk::Box bands_{Gtk::ORIENTATION_HORIZONTAL, 4};
  Gtk::Box buttons_{Gtk::ORIENTATION_HORIZONTAL, 6};
  std::array<Gtk::Scale*, Settings::kEqBands> scales_{};
  Gtk::Button reset_{"_Reset", true};
  Gtk::Button close_{"_Close", true};
  bool ignore_ = false;
};

}  // namespace earblaster
