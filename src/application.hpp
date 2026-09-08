/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <gtkmm.h>

namespace earblaster {

class Application : public Gtk::Application {
 public:
  static Glib::RefPtr<Application> create();

 protected:
  Application();
  void on_activate() override;
};

}  // namespace earblaster
