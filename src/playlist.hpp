/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <gdkmm/pixbuf.h>
#include <gtkmm.h>

#include <deque>
#include <map>
#include <random>
#include <string>
#include <vector>

typedef struct _GstDiscoverer GstDiscoverer;
typedef struct _GstDiscovererInfo GstDiscovererInfo;

namespace earblaster {

class Playlist {
 public:
  enum class Repeat { Off, All, One };

  struct Columns : public Gtk::TreeModel::ColumnRecord {
    Columns()
    {
      add(cover);
      add(title);
      add(artist);
      add(time);
      add(uri);
      add(duration_ns);
      add(start_ns);
      add(stop_ns);
      add(cue);
      add(id);
    }
    Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> cover;
    Gtk::TreeModelColumn<Glib::ustring> title;
    Gtk::TreeModelColumn<Glib::ustring> artist;
    Gtk::TreeModelColumn<Glib::ustring> time;
    Gtk::TreeModelColumn<Glib::ustring> uri;
    Gtk::TreeModelColumn<gint64> duration_ns;
    Gtk::TreeModelColumn<gint64> start_ns;
    Gtk::TreeModelColumn<gint64> stop_ns;
    Gtk::TreeModelColumn<bool> cue;
    /* Stable per-row id; survives deletes above the row and drag reorders. */
    Gtk::TreeModelColumn<gint64> id;
  };

  Playlist();
  ~Playlist();

  Playlist(const Playlist&) = delete;
  Playlist& operator=(const Playlist&) = delete;

  Glib::RefPtr<Gtk::ListStore> store() const
  {
    return store_;
  }
  const Columns& columns() const
  {
    return columns_;
  }

  void clear();
  int size() const;
  bool empty() const
  {
    return size() == 0;
  }

  int current_index() const;
  std::string current_uri() const;
  gint64 current_start_ns() const;
  gint64 current_stop_ns() const;
  Gtk::TreeModel::iterator current_iter() const;
  Gtk::TreeModel::Path current_path() const;
  void set_current(int index);
  void set_current(const Gtk::TreeModel::Path& path);

  int add_audio_file(const std::string& path);
  int add_files(const std::vector<std::string>& paths);
  int add_folder(const std::string& dir);
  int add_m3u(const std::string& path);
  int add_cue(const std::string& path);
  int add_dropped(const std::vector<Glib::ustring>& uris);
  bool save_m3u(const std::string& path) const;

  bool next();
  bool prev();

  bool shuffle() const
  {
    return shuffle_;
  }
  void set_shuffle(bool shuffle);
  Repeat repeat() const
  {
    return repeat_;
  }
  void set_repeat(Repeat repeat);

  void update_current_meta(const Glib::ustring& title, const Glib::ustring& artist,
                           gint64 duration_ns);
  /* Returns true if the current (playing) row was removed. */
  bool remove_paths(const std::vector<Gtk::TreeModel::Path>& paths);

  static bool is_audio_path(const std::string& path);
  static bool is_m3u_path(const std::string& path);
  static bool is_cue_path(const std::string& path);

 private:
  std::string iter_uri(const Gtk::TreeModel::iterator& it) const;
  int index_of_id(gint64 id) const;
  int append_uri(const std::string& uri, const Glib::ustring& title, gint64 start_ns = 0,
                 gint64 stop_ns = 0, bool cue = false, const Glib::ustring& artist = {});
  void enqueue_meta(const std::string& uri);
  void enqueue_cover(const std::string& uri);
  void schedule_meta();
  void schedule_cover();
  void pump_cover();
  bool ensure_discoverer();
  void pump_meta();
  void apply_discoverer_info(GstDiscovererInfo* info);
  static void on_discovered(GstDiscoverer* disc, GstDiscovererInfo* info, GError* err,
                            gpointer self);
  static void on_finished(GstDiscoverer* disc, gpointer self);

  Columns columns_;
  Glib::RefPtr<Gtk::ListStore> store_;
  Gtk::TreeRowReference current_;
  bool shuffle_ = false;
  Repeat repeat_ = Repeat::Off;
  std::vector<gint64> history_;
  gint64 next_id_ = 1;
  /* Row that takes over after the playing row is removed; -1 when unset. */
  int successor_ = -1;
  std::mt19937 rng_{std::random_device{}()};
  GstDiscoverer* discoverer_ = nullptr;
  std::deque<std::string> meta_queue_;
  std::deque<std::string> cover_queue_;
  std::map<std::string, Glib::RefPtr<Gdk::Pixbuf>> cover_cache_;
  bool discovering_ = false;
  sigc::connection meta_idle_;
  sigc::connection cover_idle_;
};

}  // namespace earblaster
