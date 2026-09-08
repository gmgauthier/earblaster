/* SPDX-License-Identifier: Unlicense */

#include "main_window.hpp"
#include "about_dialog.hpp"
#include "paths.hpp"
#include "config.hpp"

#include <iostream>

namespace earblaster {
namespace {

Gtk::MenuItem* add_item(Gtk::Menu& menu, const Glib::ustring& label,
                        const sigc::slot<void()>& slot)
{
  auto* item = Gtk::manage(new Gtk::MenuItem(label, true));
  item->signal_activate().connect(slot);
  menu.append(*item);
  return item;
}

}  // namespace

MainWindow::MainWindow()
{
  set_title("EarBlaster");
  set_default_size(800, 534);
  set_border_width(0);
  get_style_context()->add_class("earblaster-window");

  load_css();
  load_window_icon();
  build_menu();
  build_body();

  status_ctx_ = status_.get_context_id("main");
  sync_transport();

  add(root_);
  show_all();
}

void MainWindow::load_css()
{
  const std::string css_path = find_data_file("skin/lcos/lcos.css");
  if (css_path.empty()) {
    std::cerr << "earblaster: lcos.css not found\n";
    return;
  }
  try {
    auto css = Gtk::CssProvider::create();
    css->load_from_path(css_path);
    Gtk::StyleContext::add_provider_for_screen(
        Gdk::Screen::get_default(), css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  } catch (const Glib::Error& e) {
    std::cerr << "earblaster: CSS: " << e.what() << "\n";
  }
}

void MainWindow::load_window_icon()
{
  const std::string icon = find_data_file("brand/icon-tile.svg");
  if (icon.empty())
    return;
  try {
    set_icon_from_file(icon);
  } catch (const Glib::Error&) {
  }
}

void MainWindow::build_menu()
{
  auto add_menu = [this](const Glib::ustring& label, Gtk::Menu& menu) {
    auto* top = Gtk::manage(new Gtk::MenuItem(label, true));
    top->set_submenu(menu);
    menubar_.append(*top);
  };

  auto* file = Gtk::manage(new Gtk::Menu());
  add_item(*file, "_Open File…", sigc::mem_fun(*this, &MainWindow::on_open_file));
  add_item(*file, "Open _Folder…",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_not_yet),
                      Glib::ustring("Open Folder")));
  file->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
  add_item(*file, "_Quit", sigc::mem_fun(*this, &MainWindow::on_quit));
  add_menu("_File", *file);

  auto* edit = Gtk::manage(new Gtk::Menu());
  add_item(*edit, "_Preferences…",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_not_yet),
                      Glib::ustring("Preferences")));
  add_menu("_Edit", *edit);

  auto* view = Gtk::manage(new Gtk::Menu());
  add_item(*view, "_Playlist",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_not_yet),
                      Glib::ustring("View")));
  add_menu("_View", *view);

  auto* play = Gtk::manage(new Gtk::Menu());
  add_item(*play, "_Play / Pause",
           sigc::mem_fun(*this, &MainWindow::on_play_pause));
  add_item(*play, "_Stop", sigc::mem_fun(*this, &MainWindow::on_stop));
  add_menu("_Play", *play);

  auto* tools = Gtk::manage(new Gtk::Menu());
  add_item(*tools, "_Equalizer…",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_not_yet),
                      Glib::ustring("Equalizer")));
  add_menu("_Tools", *tools);

  auto* help = Gtk::manage(new Gtk::Menu());
  add_item(*help, "_About EarBlaster", sigc::mem_fun(*this, &MainWindow::on_about));
  add_menu("_Help", *help);

  root_.pack_start(menubar_, Gtk::PACK_SHRINK);
}

void MainWindow::build_body()
{
  body_.set_border_width(6);
  body_.set_homogeneous(false);

  well_.set_halign(Gtk::ALIGN_FILL);
  left_.pack_start(well_, Gtk::PACK_SHRINK);

  transport_.set_homogeneous(true);
  transport_.set_margin_top(4);
  btn_prev_.set_sensitive(false);
  btn_next_.set_sensitive(false);
  btn_play_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_play));
  btn_pause_.signal_clicked().connect(
      sigc::mem_fun(*this, &MainWindow::on_pause));
  btn_stop_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_stop));
  transport_.pack_start(btn_prev_);
  transport_.pack_start(btn_play_);
  transport_.pack_start(btn_pause_);
  transport_.pack_start(btn_stop_);
  transport_.pack_start(btn_next_);
  left_.pack_start(transport_, Gtk::PACK_SHRINK);

  seek_.set_range(0.0, 1.0);
  seek_.set_value(0.0);
  seek_.set_draw_value(false);
  seek_.set_sensitive(false);
  seek_.set_margin_top(6);
  left_.pack_start(seek_label_, Gtk::PACK_SHRINK);
  left_.pack_start(seek_, Gtk::PACK_SHRINK);

  volume_.set_range(0.0, 1.0);
  volume_.set_value(0.8);
  volume_.set_draw_value(false);
  volume_.set_margin_top(6);
  left_.pack_start(volume_label_, Gtk::PACK_SHRINK);
  left_.pack_start(volume_, Gtk::PACK_SHRINK);

  left_.set_size_request(260, -1);
  body_.pack_start(left_, Gtk::PACK_SHRINK);

  store_ = Gtk::ListStore::create(columns_);
  playlist_.set_model(store_);
  playlist_.append_column("Title", columns_.title);
  playlist_.append_column("Artist", columns_.artist);
  playlist_.append_column("Time", columns_.time);
  playlist_.set_headers_visible(true);
  playlist_.get_style_context()->add_class("earblaster-playlist");

  list_scroll_.add(playlist_);
  list_scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  list_scroll_.set_shadow_type(Gtk::SHADOW_IN);
  list_scroll_.set_hexpand(true);
  list_scroll_.set_vexpand(true);
  body_.pack_start(list_scroll_, Gtk::PACK_EXPAND_WIDGET);

  root_.pack_start(body_, Gtk::PACK_EXPAND_WIDGET);
  root_.pack_start(status_, Gtk::PACK_SHRINK);
}

void MainWindow::set_status(const Glib::ustring& text)
{
  status_.pop(status_ctx_);
  status_.push(text, status_ctx_);
}

void MainWindow::on_open_file()
{
  Gtk::FileChooserDialog dlg(*this, "Open File", Gtk::FILE_CHOOSER_ACTION_OPEN);
  dlg.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dlg.add_button("_Open", Gtk::RESPONSE_ACCEPT);
  auto filter = Gtk::FileFilter::create();
  filter->set_name("Audio");
  filter->add_mime_type("audio/*");
  filter->add_pattern("*.mp3");
  filter->add_pattern("*.ogg");
  filter->add_pattern("*.oga");
  filter->add_pattern("*.flac");
  filter->add_pattern("*.wav");
  filter->add_pattern("*.m4a");
  dlg.add_filter(filter);
  if (dlg.run() != Gtk::RESPONSE_ACCEPT)
    return;
  set_status("M2 will play: " + dlg.get_filename());
}

void MainWindow::on_quit()
{
  hide();
}

void MainWindow::on_about()
{
  AboutDialog dlg(*this);
  dlg.run();
}

void MainWindow::on_play()
{
  if (dummy_ == DummyState::Playing)
    return;
  dummy_ = DummyState::Playing;
  well_.set_playing(true);
  sync_transport();
}

void MainWindow::on_pause()
{
  if (dummy_ != DummyState::Playing)
    return;
  dummy_ = DummyState::Paused;
  well_.set_playing(false);
  sync_transport();
}

void MainWindow::on_stop()
{
  if (dummy_ == DummyState::Stopped)
    return;
  dummy_ = DummyState::Stopped;
  well_.stop();
  sync_transport();
}

void MainWindow::on_play_pause()
{
  if (dummy_ == DummyState::Playing)
    on_pause();
  else
    on_play();
}

void MainWindow::sync_transport()
{
  const bool playing = dummy_ == DummyState::Playing;
  const bool stopped = dummy_ == DummyState::Stopped;
  btn_play_.set_sensitive(!playing);
  btn_pause_.set_sensitive(playing);
  btn_stop_.set_sensitive(!stopped);
  if (stopped)
    set_status("Stopped — 0:00 / 0:00");
  else if (playing)
    set_status("Playing — 0:00 / 0:00");
  else
    set_status("Paused — 0:00 / 0:00");
}

void MainWindow::on_not_yet(const Glib::ustring& feature)
{
  set_status(feature + " arrives after M1.");
}

}  // namespace earblaster
