/* SPDX-License-Identifier: Unlicense */

#include "application.hpp"
#include "main_window.hpp"
#include "config.hpp"

namespace earblaster {

Glib::RefPtr<Application> Application::create()
{
  return Glib::RefPtr<Application>(new Application());
}

Application::Application()
    : Gtk::Application(APP_ID, Gio::APPLICATION_FLAGS_NONE)
{
}

void Application::on_activate()
{
  auto* win = new MainWindow();
  add_window(*win);
  win->signal_hide().connect([win]() { delete win; });
  win->present();
}

}  // namespace earblaster
