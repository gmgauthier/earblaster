/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "settings.hpp"

#include <gtkmm.h>

namespace earblaster {

class PrefsWindow : public Gtk::Dialog {
 public:
  PrefsWindow(Gtk::Window& parent, Settings& settings);
  void apply();

 private:
  Settings& settings_;
  Gtk::Box box_{Gtk::ORIENTATION_VERTICAL, 8};
  Gtk::Label folder_lab_{"Music folder"};
  Gtk::FileChooserButton folder_{"Music folder", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER};
  Gtk::CheckButton restore_win_{"Restore window position on startup"};
  Gtk::CheckButton shuffle_{"Shuffle"};
  Gtk::CheckButton repeat_{"Repeat"};
};

}  // namespace earblaster
