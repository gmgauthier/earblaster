/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "settings.hpp"
#include "sync_devices.hpp"

#include <giomm.h>
#include <gtkmm.h>

#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace earblaster {

class SyncPane : public Gtk::Box {
 public:
  explicit SyncPane(bool device_side);

  void set_ceiling(const Glib::RefPtr<Gio::File>& ceiling);
  void navigate(const Glib::RefPtr<Gio::File>& dir);
  void refresh();
  void clear_ticks();
  void set_list_sensitive(bool on);

  Glib::RefPtr<Gio::File> current() const
  {
    return current_;
  }
  std::vector<Glib::RefPtr<Gio::File>> ticked() const;
  Gtk::ComboBoxText& combo()
  {
    return combo_;
  }

  sigc::signal<void, Glib::ustring> signal_status;

 private:
  struct Columns : public Gtk::TreeModel::ColumnRecord {
    Columns()
    {
      add(tick);
      add(tickable);
      add(is_dir);
      add(is_parent);
      add(name);
      add(size_text);
      add(uri);
      add(weight);
    }
    Gtk::TreeModelColumn<bool> tick;
    Gtk::TreeModelColumn<bool> tickable;
    Gtk::TreeModelColumn<bool> is_dir;
    Gtk::TreeModelColumn<bool> is_parent;
    Gtk::TreeModelColumn<Glib::ustring> name;
    Gtk::TreeModelColumn<Glib::ustring> size_text;
    Gtk::TreeModelColumn<Glib::ustring> uri;
    Gtk::TreeModelColumn<int> weight;
  };

  void on_toggled(const Glib::ustring& path);
  void on_row_activated(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* col);
  bool at_ceiling() const;

  bool device_side_ = false;
  Columns cols_;
  Glib::RefPtr<Gtk::ListStore> store_;
  Glib::RefPtr<Gio::File> ceiling_;
  Glib::RefPtr<Gio::File> current_;
  Gtk::Box head_{Gtk::ORIENTATION_HORIZONTAL, 0};
  Gtk::Label title_;
  Gtk::ComboBoxText combo_;
  Gtk::Label path_;
  Gtk::ScrolledWindow scroll_;
  Gtk::TreeView view_;
};

class SyncWindow : public Gtk::Window {
 public:
  SyncWindow(Gtk::Window& parent, Settings& settings);
  ~SyncWindow() override;

  void refresh_devices();

 private:
  void set_status(const Glib::ustring& text);
  void on_device_changed();
  void on_transfer();
  void on_close();
  void run_copy();
  void on_copy_progress();
  void on_copy_done();
  void stop_copy();
  void set_busy(bool on);
  bool dest_writable(const Glib::RefPtr<Gio::File>& dir) const;
  bool on_key_press(GdkEventKey* event);

  Settings& settings_;
  Gtk::Box root_{Gtk::ORIENTATION_VERTICAL, 6};
  Gtk::Paned panes_{Gtk::ORIENTATION_HORIZONTAL};
  SyncPane left_{false};
  SyncPane right_{true};
  Gtk::Box actions_{Gtk::ORIENTATION_HORIZONTAL, 8};
  Gtk::Label status_;
  Gtk::Button transfer_{"_Transfer", true};
  Gtk::Button close_{"_Close", true};

  std::vector<SyncDevice> devices_;
  bool ignore_device_ = false;
  bool busy_ = false;

  std::vector<std::string> job_srcs_;
  std::string job_dest_;
  Glib::RefPtr<Gio::Cancellable> cancellable_;
  std::thread copy_thread_;
  std::mutex mu_;
  Glib::ustring progress_text_;
  Glib::ustring result_text_;
  Glib::Dispatcher progress_;
  Glib::Dispatcher done_;
  sigc::connection progress_conn_;
  sigc::connection done_conn_;
  Glib::RefPtr<Gio::VolumeMonitor> monitor_;
};

}  // namespace earblaster
