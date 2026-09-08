/* SPDX-License-Identifier: Unlicense */

#include "prefs_window.hpp"

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

namespace earblaster {

PrefsWindow::PrefsWindow(Gtk::Window& parent, Settings& settings)
    : Gtk::Dialog("Preferences", parent, true), settings_(settings)
{
  set_border_width(8);
  set_resizable(false);
  add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  add_button("_OK", Gtk::RESPONSE_OK);
  set_default_response(Gtk::RESPONSE_OK);

  std::string start = settings_.music_dir;
  if (start.empty() || !Glib::file_test(start, Glib::FILE_TEST_IS_DIR)) {
    start = Glib::build_filename(Glib::get_home_dir(), "Music");
    if (!Glib::file_test(start, Glib::FILE_TEST_IS_DIR))
      start = Glib::get_home_dir();
  }
  folder_.set_current_folder(start);
  restore_win_.set_active(settings_.restore_window);
  shuffle_.set_active(settings_.shuffle);
  repeat_.set_active(settings_.repeat);

  box_.pack_start(folder_lab_, Gtk::PACK_SHRINK);
  box_.pack_start(folder_, Gtk::PACK_SHRINK);
  box_.pack_start(restore_win_, Gtk::PACK_SHRINK);
  box_.pack_start(shuffle_, Gtk::PACK_SHRINK);
  box_.pack_start(repeat_, Gtk::PACK_SHRINK);
  get_content_area()->pack_start(box_, Gtk::PACK_EXPAND_WIDGET);
  show_all();
}

void PrefsWindow::apply()
{
  const std::string dir = folder_.get_filename();
  if (!dir.empty())
    settings_.music_dir = dir;
  settings_.restore_window = restore_win_.get_active();
  settings_.shuffle = shuffle_.get_active();
  settings_.repeat = repeat_.get_active();
}

}  // namespace earblaster
