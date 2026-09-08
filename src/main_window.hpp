/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "player.hpp"
#include "seal_view.hpp"

#include <gtkmm.h>

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

  void on_open_file();
  void on_quit();
  void on_about();
  void on_play();
  void on_pause();
  void on_stop();
  void on_play_pause();
  void sync_transport();
  void update_clock();
  void on_player_state(Player::State state);
  void on_player_position(gint64 position, gint64 duration);
  void on_player_error(const Glib::ustring& message);
  bool on_seek_press(GdkEventButton* event);
  bool on_seek_release(GdkEventButton* event);
  void on_volume_changed();
  void on_not_yet(const Glib::ustring& feature);

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
  Gtk::TreeView playlist_;
  Glib::RefPtr<Gtk::ListStore> store_;
  Gtk::Statusbar status_;
  guint status_ctx_ = 0;
  Player player_;
  bool seek_dragging_ = false;

  struct Columns : public Gtk::TreeModel::ColumnRecord {
    Columns()
    {
      add(title);
      add(artist);
      add(time);
    }
    Gtk::TreeModelColumn<Glib::ustring> title;
    Gtk::TreeModelColumn<Glib::ustring> artist;
    Gtk::TreeModelColumn<Glib::ustring> time;
  } columns_;
};

}  // namespace earblaster
