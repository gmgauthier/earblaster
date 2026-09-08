/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "cover_art.hpp"
#include "eq_window.hpp"
#include "prefs_window.hpp"
#include "player.hpp"
#include "playlist.hpp"
#include "seal_view.hpp"
#include "settings.hpp"

#include <gtkmm.h>

#include <memory>

namespace earblaster {

class MainWindow : public Gtk::Window {
 public:
  MainWindow();

 private:
  void build_menu();
  void build_body();
  void load_css();
  void load_window_icon();
  void set_status(const Glib::ustring& text);

  void on_new_file();
  void on_add_file();
  void on_new_folder();
  void on_add_folder();
  void on_new_playlist();
  void on_add_playlist();
  void on_save_playlist();
  void on_remove_rows();
  void on_quit();
  void on_about();
  void on_equalizer();
  void on_preferences();
  std::string chooser_start_dir() const;
  void persist();
  bool on_key_press(GdkEventKey* event);
  void seek_relative(int seconds);
  bool on_window_delete(GdkEventAny* event);
  void on_play();
  void on_pause();
  void on_stop();
  void on_play_pause();
  void on_prev();
  void on_next();
  void on_shuffle();
  void on_repeat();
  void sync_transport();
  void update_clock();
  void play_current();
  void select_current_row();
  void on_row_activated(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* col);
  void on_eos();
  void on_player_state(Player::State state);
  void on_player_position(gint64 position, gint64 duration);
  void on_player_error(const Glib::ustring& message);
  void on_player_tags(const Glib::ustring& title, const Glib::ustring& artist);
  void on_player_cover(const Glib::RefPtr<Gdk::Pixbuf>& pix);
  bool on_seek_press(GdkEventButton* event);
  bool on_seek_release(GdkEventButton* event);
  void on_volume_changed();
  void on_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& ctx, int x, int y,
                             const Gtk::SelectionData& data, guint info, guint time);
  bool on_list_key_press(GdkEventKey* event);
  void on_not_yet(const Glib::ustring& feature);

  std::vector<std::string> choose_audio_files();
  std::string choose_folder(const Glib::ustring& title);
  std::string choose_m3u(bool save);

  Gtk::Box root_{Gtk::ORIENTATION_VERTICAL, 0};
  Gtk::MenuBar menubar_;
  Gtk::Box body_{Gtk::ORIENTATION_HORIZONTAL, 6};
  Gtk::Box left_{Gtk::ORIENTATION_VERTICAL, 4};
  SealView well_;
  Gtk::Box transport_{Gtk::ORIENTATION_HORIZONTAL, 4};
  Gtk::Button btn_prev_{"<<"};
  Gtk::Button btn_play_{">"};
  Gtk::Button btn_pause_{"||"};
  Gtk::Button btn_stop_{"[]"};
  Gtk::Button btn_next_{">>"};
  Gtk::Label seek_label_{"Seek"};
  Gtk::Scale seek_{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Label volume_label_{"Volume"};
  Gtk::Scale volume_{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::ScrolledWindow list_scroll_;
  Gtk::TreeView playlist_view_;
  Gtk::Statusbar status_;
  guint status_ctx_ = 0;
  Settings settings_;
  Player player_;
  Playlist playlist_;
  std::unique_ptr<EqWindow> eq_win_;
  bool seek_dragging_ = false;
  bool have_local_cover_ = false;
  Gtk::CheckMenuItem* shuffle_item_ = nullptr;
  Gtk::CheckMenuItem* repeat_item_ = nullptr;
};

}  // namespace earblaster
