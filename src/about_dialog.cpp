/* SPDX-License-Identifier: Unlicense */

#include "about_dialog.hpp"
#include "paths.hpp"
#include "config.hpp"

namespace earblaster {

AboutDialog::AboutDialog(Gtk::Window& parent)
    : Gtk::Dialog("About EarBlaster", parent, true)
{
  set_resizable(false);
  add_button("_OK", Gtk::RESPONSE_OK);
  set_default_response(Gtk::RESPONSE_OK);

  auto* box = get_content_area();
  box->set_border_width(16);
  box->set_spacing(10);

  const std::string lockup = find_data_file("brand/lockup-pill.svg");
  if (!lockup.empty()) {
    try {
      auto pix = Gdk::Pixbuf::create_from_file(lockup, 360, -1);
      auto* image = Gtk::manage(new Gtk::Image(pix));
      box->pack_start(*image, Gtk::PACK_SHRINK);
    } catch (const Glib::Error&) {
    }
  }

  auto* title = Gtk::manage(new Gtk::Label());
  title->set_markup("<b>EarBlaster " VERSION "</b>");
  box->pack_start(*title, Gtk::PACK_SHRINK);

  auto* line = Gtk::manage(new Gtk::Label(
      "EarBlaster — a media player for The Lunduke Computer Operating System."));
  line->set_line_wrap(true);
  line->set_max_width_chars(52);
  box->pack_start(*line, Gtk::PACK_SHRINK);

  auto* guest = Gtk::manage(new Gtk::Label(
      "Third-party software written for LCOS. Not LCOS house software."));
  guest->set_line_wrap(true);
  guest->set_max_width_chars(52);
  box->pack_start(*guest, Gtk::PACK_SHRINK);

  auto* license = Gtk::manage(new Gtk::Label("Released under The Unlicense."));
  box->pack_start(*license, Gtk::PACK_SHRINK);

  show_all_children();
}

}  // namespace earblaster
