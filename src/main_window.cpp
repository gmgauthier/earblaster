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

void add_audio_filter(Gtk::FileChooserDialog& dlg)
{
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
}

void add_m3u_filter(Gtk::FileChooserDialog& dlg)
{
  auto filter = Gtk::FileFilter::create();
  filter->set_name("Playlist");
  filter->add_pattern("*.m3u");
  filter->add_pattern("*.m3u8");
  dlg.add_filter(filter);
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
  player_.signal_cover().connect(sigc::mem_fun(well_, &SealView::set_cover));
  player_.signal_eos().connect(sigc::mem_fun(*this, &MainWindow::on_eos));
  player_.signal_tags().connect(sigc::mem_fun(*this, &MainWindow::on_player_tags));
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
  add_item(*file, "_New File…", sigc::mem_fun(*this, &MainWindow::on_new_file));
  add_item(*file, "New _Folder…", sigc::mem_fun(*this, &MainWindow::on_new_folder));
  add_item(*file, "New P_laylist…",
           sigc::mem_fun(*this, &MainWindow::on_new_playlist));
  file->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
  add_item(*file, "_Add File…", sigc::mem_fun(*this, &MainWindow::on_add_file));
  add_item(*file, "Add Fol_der…", sigc::mem_fun(*this, &MainWindow::on_add_folder));
  add_item(*file, "Add Playlis_t…",
           sigc::mem_fun(*this, &MainWindow::on_add_playlist));
  file->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
  add_item(*file, "_Save Playlist…",
           sigc::mem_fun(*this, &MainWindow::on_save_playlist));
  file->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
  add_item(*file, "_Quit", sigc::mem_fun(*this, &MainWindow::on_quit));
  add_menu("_File", *file);

  auto* edit = Gtk::manage(new Gtk::Menu());
  add_item(*edit, "_Remove", sigc::mem_fun(*this, &MainWindow::on_remove_rows));
  edit->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
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
  play->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
  add_item(*play, "P_revious", sigc::mem_fun(*this, &MainWindow::on_prev));
  add_item(*play, "_Next", sigc::mem_fun(*this, &MainWindow::on_next));
  play->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
  shuffle_item_ = Gtk::manage(new Gtk::CheckMenuItem("Sh_uffle"));
  shuffle_item_->signal_toggled().connect(
      sigc::mem_fun(*this, &MainWindow::on_shuffle));
  play->append(*shuffle_item_);
  repeat_item_ = Gtk::manage(new Gtk::CheckMenuItem("R_epeat"));
  repeat_item_->signal_toggled().connect(
      sigc::mem_fun(*this, &MainWindow::on_repeat));
  play->append(*repeat_item_);
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
  btn_play_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_play));
  btn_pause_.signal_clicked().connect(
      sigc::mem_fun(*this, &MainWindow::on_pause));
  btn_stop_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_stop));
  btn_prev_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_prev));
  btn_next_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_next));
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

  playlist_view_.set_model(playlist_.store());
  auto add_col = [this](const char* name, const Gtk::TreeModelColumn<Glib::ustring>& model_col,
                        bool expand, int min_width) {
    auto* rend = Gtk::manage(new Gtk::CellRendererText());
    rend->property_ellipsize() = Pango::ELLIPSIZE_END;
    auto* col = Gtk::manage(new Gtk::TreeViewColumn(name, *rend));
    col->add_attribute(rend->property_text(), model_col);
    col->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
    col->set_resizable(true);
    col->set_expand(expand);
    col->set_min_width(min_width);
    playlist_view_.append_column(*col);
  };
  add_col("Title", playlist_.columns().title, true, 80);
  add_col("Artist", playlist_.columns().artist, true, 60);
  add_col("Time", playlist_.columns().time, false, 48);
  playlist_view_.set_fixed_height_mode(true);
  playlist_view_.set_headers_visible(true);
  playlist_view_.get_style_context()->add_class("earblaster-playlist");
  playlist_view_.set_reorderable(true);
  playlist_view_.get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
  playlist_view_.signal_row_activated().connect(
      sigc::mem_fun(*this, &MainWindow::on_row_activated));
  playlist_view_.signal_key_press_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_list_key_press), false);

  std::vector<Gtk::TargetEntry> targets = {
      Gtk::TargetEntry("text/uri-list", Gtk::TargetFlags(0), 0)};
  playlist_view_.drag_dest_set(targets, Gtk::DEST_DEFAULT_ALL, Gdk::ACTION_COPY);
  playlist_view_.signal_drag_data_received().connect(
      sigc::mem_fun(*this, &MainWindow::on_drag_data_received));

  list_scroll_.add(playlist_view_);
  list_scroll_.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
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

std::vector<std::string> MainWindow::choose_audio_files()
{
  Gtk::FileChooserDialog dlg(*this, "Select Audio", Gtk::FILE_CHOOSER_ACTION_OPEN);
  dlg.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dlg.add_button("_Open", Gtk::RESPONSE_ACCEPT);
  dlg.set_select_multiple(true);
  add_audio_filter(dlg);
  dlg.set_current_folder(std::string(SOURCE_ROOT) + "/data/samples");
  if (dlg.run() != Gtk::RESPONSE_ACCEPT)
    return {};
  return dlg.get_filenames();
}

std::string MainWindow::choose_folder(const Glib::ustring& title)
{
  Gtk::FileChooserDialog dlg(*this, title, Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
  dlg.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dlg.add_button("_Open", Gtk::RESPONSE_ACCEPT);
  dlg.set_current_folder(std::string(SOURCE_ROOT) + "/data/samples");
  if (dlg.run() != Gtk::RESPONSE_ACCEPT)
    return {};
  return dlg.get_filename();
}

std::string MainWindow::choose_m3u(bool save)
{
  Gtk::FileChooserDialog dlg(*this, save ? "Save Playlist" : "Select Playlist",
                             save ? Gtk::FILE_CHOOSER_ACTION_SAVE
                                  : Gtk::FILE_CHOOSER_ACTION_OPEN);
  dlg.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dlg.add_button(save ? "_Save" : "_Open", Gtk::RESPONSE_ACCEPT);
  add_m3u_filter(dlg);
  if (save)
    dlg.set_do_overwrite_confirmation(true);
  if (dlg.run() != Gtk::RESPONSE_ACCEPT)
    return {};
  return dlg.get_filename();
}

void MainWindow::play_current()
{
  const std::string uri = playlist_.current_uri();
  if (uri.empty())
    return;
  if (!player_.open(uri))
    return;
  well_.stop();
  player_.play();
  select_current_row();
}

void MainWindow::select_current_row()
{
  const auto path = playlist_.current_path();
  if (path.empty())
    return;
  auto sel = playlist_view_.get_selection();
  sel->unselect_all();
  sel->select(path);
  playlist_view_.scroll_to_row(path);
}

void MainWindow::on_new_file()
{
  const auto files = choose_audio_files();
  if (files.empty())
    return;
  playlist_.clear();
  if (playlist_.add_files(files) <= 0) {
    set_status("No audio files in that selection.");
    sync_transport();
    return;
  }
  playlist_.set_current(0);
  play_current();
}

void MainWindow::on_add_file()
{
  const auto files = choose_audio_files();
  if (files.empty())
    return;
  const int n = playlist_.add_files(files);
  if (n <= 0)
    set_status("No audio files in that selection.");
  sync_transport();
}

void MainWindow::on_new_folder()
{
  const auto dir = choose_folder("New Folder");
  if (dir.empty())
    return;
  playlist_.clear();
  if (playlist_.add_folder(dir) <= 0) {
    set_status("No audio files in that folder.");
    sync_transport();
    return;
  }
  playlist_.set_current(0);
  play_current();
}

void MainWindow::on_add_folder()
{
  const auto dir = choose_folder("Add Folder");
  if (dir.empty())
    return;
  if (playlist_.add_folder(dir) <= 0)
    set_status("No audio files in that folder.");
  sync_transport();
}

void MainWindow::on_new_playlist()
{
  const auto path = choose_m3u(false);
  if (path.empty())
    return;
  playlist_.clear();
  if (playlist_.add_m3u(path) <= 0) {
    set_status("Playlist had no playable audio.");
    sync_transport();
    return;
  }
  playlist_.set_current(0);
  play_current();
}

void MainWindow::on_add_playlist()
{
  const auto path = choose_m3u(false);
  if (path.empty())
    return;
  if (playlist_.add_m3u(path) <= 0)
    set_status("Playlist had no playable audio.");
  sync_transport();
}

void MainWindow::on_save_playlist()
{
  if (playlist_.empty()) {
    set_status("Playlist is empty.");
    return;
  }
  auto path = choose_m3u(true);
  if (path.empty())
    return;
  if (Playlist::is_m3u_path(path) == false)
    path += ".m3u";
  if (!playlist_.save_m3u(path))
    set_status("Could not save playlist.");
}

void MainWindow::on_remove_rows()
{
  const auto rows = playlist_view_.get_selection()->get_selected_rows();
  if (rows.empty())
    return;
  const bool killed = playlist_.remove_paths(rows);
  if (killed) {
    if (playlist_.next())
      play_current();
    else {
      player_.stop();
      well_.stop();
    }
  }
  sync_transport();
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
  if (playlist_.empty()) {
    on_new_file();
    return;
  }
  if (player_.state() == Player::State::Playing)
    return;
  if (!player_.loaded() || playlist_.current_uri().empty()) {
    if (playlist_.current_index() < 0)
      playlist_.set_current(0);
    play_current();
    return;
  }
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

void MainWindow::on_prev()
{
  if (!playlist_.prev())
    return;
  play_current();
}

void MainWindow::on_next()
{
  if (!playlist_.next())
    return;
  play_current();
}

void MainWindow::on_shuffle()
{
  playlist_.set_shuffle(shuffle_item_ && shuffle_item_->get_active());
}

void MainWindow::on_repeat()
{
  playlist_.set_repeat((repeat_item_ && repeat_item_->get_active())
                           ? Playlist::Repeat::All
                           : Playlist::Repeat::Off);
}

void MainWindow::on_row_activated(const Gtk::TreeModel::Path& path,
                                  Gtk::TreeViewColumn*)
{
  playlist_.set_current(path);
  play_current();
}

void MainWindow::on_eos()
{
  if (playlist_.repeat() == Playlist::Repeat::One) {
    player_.seek(0);
    player_.play();
    return;
  }
  if (playlist_.next()) {
    play_current();
    return;
  }
  player_.stop();
  well_.stop();
}

void MainWindow::sync_transport()
{
  const auto state = player_.state();
  const bool playing = state == Player::State::Playing;
  const bool stopped = state == Player::State::Stopped;
  const bool has_list = !playlist_.empty();
  btn_play_.set_sensitive(!playing);
  btn_pause_.set_sensitive(playing);
  btn_stop_.set_sensitive(!stopped);
  btn_prev_.set_sensitive(has_list);
  btn_next_.set_sensitive(has_list);
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
  playlist_.update_current_meta("", "", duration);
  update_clock();
}

void MainWindow::on_player_error(const Glib::ustring& message)
{
  set_status("Error — " + message);
}

void MainWindow::on_player_tags(const Glib::ustring& title, const Glib::ustring& artist)
{
  playlist_.update_current_meta(title, artist, 0);
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

void MainWindow::on_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& ctx,
                                       int, int, const Gtk::SelectionData& data,
                                       guint, guint time)
{
  const int n = playlist_.add_dropped(data.get_uris());
  ctx->drag_finish(n > 0, false, time);
  if (n <= 0)
    set_status("Nothing playable in that drop.");
  sync_transport();
}

bool MainWindow::on_list_key_press(GdkEventKey* event)
{
  if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete) {
    on_remove_rows();
    return true;
  }
  return false;
}

void MainWindow::on_not_yet(const Glib::ustring& feature)
{
  set_status(feature + " arrives after M3.");
}

}  // namespace earblaster
