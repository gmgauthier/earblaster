/* SPDX-License-Identifier: Unlicense */

#include "main_window.hpp"
#include "about_dialog.hpp"
#include "paths.hpp"
#include "config.hpp"

#include <cstdio>
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

Glib::ustring format_clock(gint64 ns)
{
  if (ns < 0)
    ns = 0;
  const int total = static_cast<int>(ns / GST_SECOND);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%d:%02d", total / 60, total % 60);
  return buf;
}

const char* state_word(Player::State state)
{
  switch (state) {
    case Player::State::Playing:
      return "Playing";
    case Player::State::Paused:
      return "Paused";
    case Player::State::Stopped:
    default:
      return "Stopped";
  }
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

  player_.signal_state_changed().connect(
      sigc::mem_fun(*this, &MainWindow::on_player_state));
  player_.signal_position_changed().connect(
      sigc::mem_fun(*this, &MainWindow::on_player_position));
  player_.signal_error().connect(
      sigc::mem_fun(*this, &MainWindow::on_player_error));
  player_.signal_cover().connect(
      sigc::mem_fun(well_, &SealView::set_cover));
  player_.set_volume(volume_.get_value());

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
  add_item(*file, "_New File…", sigc::mem_fun(*this, &MainWindow::on_open_file));
  add_item(*file, "New _Folder…",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_not_yet),
                      Glib::ustring("New Folder")));
  add_item(*file, "New P_laylist…",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_not_yet),
                      Glib::ustring("New Playlist")));
  file->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
  add_item(*file, "_Add File…",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_not_yet),
                      Glib::ustring("Add File")));
  add_item(*file, "Add Fol_der…",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_not_yet),
                      Glib::ustring("Add Folder")));
  add_item(*file, "Add Playlis_t…",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_not_yet),
                      Glib::ustring("Add Playlist")));
  file->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
  add_item(*file, "_Save Playlist…",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_not_yet),
                      Glib::ustring("Save Playlist")));
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
  seek_.signal_button_press_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_seek_press), false);
  seek_.signal_button_release_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_seek_release), false);
  left_.pack_start(seek_label_, Gtk::PACK_SHRINK);
  left_.pack_start(seek_, Gtk::PACK_SHRINK);

  volume_.set_range(0.0, 1.0);
  volume_.set_value(0.8);
  volume_.set_draw_value(false);
  volume_.set_margin_top(6);
  volume_.signal_value_changed().connect(
      sigc::mem_fun(*this, &MainWindow::on_volume_changed));
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
  Gtk::FileChooserDialog dlg(*this, "New File", Gtk::FILE_CHOOSER_ACTION_OPEN);
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
  dlg.set_current_folder(std::string(SOURCE_ROOT) + "/data/samples");
  if (dlg.run() != Gtk::RESPONSE_ACCEPT)
    return;
  if (!player_.open(dlg.get_filename()))
    return;
  well_.stop();
  player_.play();
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
  if (!player_.loaded()) {
    on_open_file();
    return;
  }
  if (player_.state() == Player::State::Playing)
    return;
  player_.play();
}

void MainWindow::on_pause()
{
  player_.pause();
}

void MainWindow::on_stop()
{
  player_.stop();
  well_.stop();
}

void MainWindow::on_play_pause()
{
  if (player_.state() == Player::State::Playing)
    on_pause();
  else
    on_play();
}

void MainWindow::sync_transport()
{
  const auto state = player_.state();
  const bool playing = state == Player::State::Playing;
  const bool stopped = state == Player::State::Stopped;
  btn_play_.set_sensitive(!playing);
  btn_pause_.set_sensitive(playing);
  btn_stop_.set_sensitive(!stopped);
  seek_.set_sensitive(player_.duration() > 0);
  update_clock();
}

void MainWindow::update_clock()
{
  set_status(Glib::ustring(state_word(player_.state())) + " — " +
             format_clock(player_.position()) + " / " +
             format_clock(player_.duration()));
}

void MainWindow::on_player_state(Player::State state)
{
  if (state == Player::State::Playing)
    well_.set_playing(true);
  else if (state == Player::State::Paused)
    well_.set_playing(false);
  else
    well_.stop();
  sync_transport();
}

void MainWindow::on_player_position(gint64 position, gint64 duration)
{
  if (!seek_dragging_ && duration > 0) {
    seek_.set_value(static_cast<double>(position) / static_cast<double>(duration));
  }
  seek_.set_sensitive(duration > 0);
  update_clock();
}

void MainWindow::on_player_error(const Glib::ustring& message)
{
  set_status("Error — " + message);
}

bool MainWindow::on_seek_press(GdkEventButton*)
{
  seek_dragging_ = true;
  return false;
}

bool MainWindow::on_seek_release(GdkEventButton*)
{
  seek_dragging_ = false;
  const gint64 dur = player_.duration();
  if (dur > 0)
    player_.seek(static_cast<gint64>(seek_.get_value() * static_cast<double>(dur)));
  return false;
}

void MainWindow::on_volume_changed()
{
  player_.set_volume(volume_.get_value());
}

void MainWindow::on_not_yet(const Glib::ustring& feature)
{
  set_status(feature + " arrives after M2.");
}

}  // namespace earblaster
