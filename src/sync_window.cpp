/* SPDX-License-Identifier: Unlicense */

#include "sync_window.hpp"

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include <algorithm>
#include <cstdio>
#include <utility>

namespace earblaster {
namespace {

const char* kListAttrs =
    "standard::name,standard::type,standard::size,standard::display-name,standard::is-hidden";

Glib::ustring format_size(guint64 n)
{
  char buf[32];
  if (n < 1024) {
    std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(n));
  } else if (n < 1024ull * 1024ull) {
    std::snprintf(buf, sizeof(buf), "%llu KB", static_cast<unsigned long long>((n + 512) / 1024));
  } else if (n < 1024ull * 1024ull * 1024ull) {
    const double mb = static_cast<double>(n) / (1024.0 * 1024.0);
    std::snprintf(buf, sizeof(buf), "%.1f MB", mb);
  } else {
    const double gb = static_cast<double>(n) / (1024.0 * 1024.0 * 1024.0);
    std::snprintf(buf, sizeof(buf), "%.1f GB", gb);
  }
  return buf;
}

Glib::RefPtr<Gio::File> local_start(const Settings& settings)
{
  if (!settings.music_dir.empty() && Glib::file_test(settings.music_dir, Glib::FILE_TEST_IS_DIR))
    return Gio::File::create_for_path(settings.music_dir);
  const std::string home_music = Glib::build_filename(Glib::get_home_dir(), "Music");
  if (Glib::file_test(home_music, Glib::FILE_TEST_IS_DIR))
    return Gio::File::create_for_path(home_music);
  return Gio::File::create_for_path(Glib::get_home_dir());
}

bool name_skipped(const std::string& name)
{
  return name.empty() || name[0] == '.' || name == ".." || name == ".";
}

/* MTP (and some FUSE backends) omit attributes even when requested.
 * is_hidden() CRITICAL-aborts if standard::is-hidden is missing. */
bool info_hidden(const Glib::RefPtr<Gio::FileInfo>& info)
{
  if (!info || !info->has_attribute("standard::is-hidden"))
    return false;
  return info->is_hidden();
}

bool dir_type(Gio::FileType type)
{
  return type == Gio::FILE_TYPE_DIRECTORY || type == Gio::FILE_TYPE_MOUNTABLE;
}

/* Returns false if dest already has this name (file or folder). Does not merge. */
bool copy_tree(const Glib::RefPtr<Gio::File>& src, const Glib::RefPtr<Gio::File>& dest_dir,
               const Glib::RefPtr<Gio::Cancellable>& cancellable,
               const sigc::slot<void, Glib::ustring>& on_file)
{
  if (!src || !dest_dir)
    return false;
  if (cancellable && cancellable->is_cancelled())
    return false;
  auto info = src->query_info(cancellable, kListAttrs);
  if (!info)
    return false;
  const auto type = info->get_file_type();
  const std::string name = src->get_basename();
  if (name_skipped(name))
    return false;
  auto dest = dest_dir->get_child(name);
  if (dest->query_exists(cancellable))
    return false;
  if (dir_type(type)) {
    dest->make_directory(cancellable);
    auto en = src->enumerate_children(cancellable, kListAttrs);
    if (!en)
      return true;
    while (auto child = en->next_file(cancellable)) {
      if (cancellable && cancellable->is_cancelled())
        return true;
      const std::string child_name = child->get_name();
      if (name_skipped(child_name) || info_hidden(child))
        continue;
      const auto ct = child->get_file_type();
      if (dir_type(ct) || ct == Gio::FILE_TYPE_REGULAR)
        copy_tree(src->get_child(child_name), dest, cancellable, on_file);
    }
    return true;
  }
  if (type != Gio::FILE_TYPE_REGULAR)
    return false;
  on_file(Glib::ustring(info->get_display_name().empty() ? name : info->get_display_name()));
  src->copy(dest, [](goffset, goffset) {}, cancellable, Gio::FILE_COPY_NONE);
  return true;
}

}  // namespace

SyncPane::SyncPane(bool device_side)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0),
      device_side_(device_side)
{
  store_ = Gtk::ListStore::create(cols_);

  get_style_context()->add_class("earblaster-sync-pane");
  head_.get_style_context()->add_class("earblaster-sync-head");
  path_.get_style_context()->add_class("earblaster-sync-path");
  path_.set_halign(Gtk::ALIGN_START);
  path_.set_ellipsize(Pango::ELLIPSIZE_START);
  path_.set_margin_start(8);
  path_.set_margin_end(8);
  path_.set_margin_top(3);
  path_.set_margin_bottom(3);

  title_.set_halign(Gtk::ALIGN_START);
  title_.set_margin_start(8);
  title_.set_margin_top(4);
  title_.set_margin_bottom(4);
  combo_.set_hexpand(true);
  combo_.set_margin_start(4);
  combo_.set_margin_end(4);
  combo_.set_margin_top(2);
  combo_.set_margin_bottom(2);

  if (device_side_) {
    title_.set_no_show_all();
    title_.hide();
    head_.pack_start(combo_, Gtk::PACK_EXPAND_WIDGET);
  } else {
    title_.set_markup("<b>This computer</b>");
    combo_.set_no_show_all();
    combo_.hide();
    head_.pack_start(title_, Gtk::PACK_EXPAND_WIDGET);
  }

  view_.set_model(store_);
  view_.set_headers_visible(false);
  view_.set_enable_search(true);
  view_.set_search_column(cols_.name);
  view_.get_style_context()->add_class("earblaster-playlist");

  auto* tick = Gtk::manage(new Gtk::CellRendererToggle());
  tick->signal_toggled().connect(sigc::mem_fun(*this, &SyncPane::on_toggled));
  auto* tick_col = Gtk::manage(new Gtk::TreeViewColumn());
  tick_col->pack_start(*tick, false);
  tick_col->add_attribute(tick->property_active(), cols_.tick);
  tick_col->add_attribute(tick->property_visible(), cols_.tickable);
  tick_col->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
  tick_col->set_fixed_width(28);
  view_.append_column(*tick_col);

  auto* name = Gtk::manage(new Gtk::CellRendererText());
  name->property_ellipsize() = Pango::ELLIPSIZE_END;
  auto* name_col = Gtk::manage(new Gtk::TreeViewColumn());
  name_col->pack_start(*name, true);
  name_col->add_attribute(name->property_text(), cols_.name);
  name_col->add_attribute(name->property_weight(), cols_.weight);
  name_col->set_expand(true);
  view_.append_column(*name_col);

  auto* sz = Gtk::manage(new Gtk::CellRendererText());
  sz->property_xalign() = 1.0;
  auto* sz_col = Gtk::manage(new Gtk::TreeViewColumn());
  sz_col->pack_start(*sz, false);
  sz_col->add_attribute(sz->property_text(), cols_.size_text);
  sz_col->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
  sz_col->set_fixed_width(72);
  view_.append_column(*sz_col);

  view_.signal_row_activated().connect(sigc::mem_fun(*this, &SyncPane::on_row_activated));

  scroll_.add(view_);
  scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  scroll_.set_shadow_type(Gtk::SHADOW_IN);
  scroll_.set_hexpand(true);
  scroll_.set_vexpand(true);

  pack_start(head_, Gtk::PACK_SHRINK);
  pack_start(path_, Gtk::PACK_SHRINK);
  pack_start(scroll_, Gtk::PACK_EXPAND_WIDGET);
}

void SyncPane::set_ceiling(const Glib::RefPtr<Gio::File>& ceiling)
{
  ceiling_ = ceiling;
}

void SyncPane::navigate(const Glib::RefPtr<Gio::File>& dir)
{
  current_ = dir;
  if (dir && ceiling_ && !(dir->equal(ceiling_) || dir->has_prefix(ceiling_)))
    current_.reset();
  refresh();
}

bool SyncPane::at_ceiling() const
{
  if (!current_)
    return true;
  if (!ceiling_)
    return !current_->get_parent();
  return current_->equal(ceiling_);
}

void SyncPane::refresh()
{
  store_->clear();
  if (!current_) {
    path_.set_text("");
    return;
  }
  path_.set_text(current_->get_parse_name());

  if (!at_ceiling()) {
    auto parent = current_->get_parent();
    if (parent) {
      auto row = *store_->append();
      row[cols_.tick] = false;
      row[cols_.tickable] = false;
      row[cols_.is_dir] = true;
      row[cols_.is_parent] = true;
      row[cols_.name] = "[..]";
      row[cols_.size_text] = "";
      row[cols_.uri] = parent->get_uri();
      row[cols_.weight] = Pango::WEIGHT_BOLD;
    }
  }

  struct Item {
    bool is_dir = false;
    Glib::ustring name;
    Glib::ustring size_text;
    Glib::ustring uri;
  };
  std::vector<Item> items;
  try {
    auto en = current_->enumerate_children(kListAttrs);
    if (!en) {
      signal_status.emit("Cannot read that folder.");
      return;
    }
    while (auto info = en->next_file()) {
      const std::string name = info->get_name();
      if (name_skipped(name) || info_hidden(info))
        continue;
      const auto type = info->get_file_type();
      if (!dir_type(type) && type != Gio::FILE_TYPE_REGULAR)
        continue;
      Item it;
      it.is_dir = dir_type(type);
      const std::string display = info->get_display_name();
      it.name = display.empty() ? Glib::ustring(name) : Glib::ustring(display);
      it.size_text =
          it.is_dir ? Glib::ustring() : format_size(static_cast<guint64>(info->get_size()));
      it.uri = current_->get_child(name)->get_uri();
      items.push_back(std::move(it));
    }
  } catch (const Glib::Error& e) {
    signal_status.emit(e.what());
    return;
  }

  std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
    if (a.is_dir != b.is_dir)
      return a.is_dir;
    return a.name.lowercase() < b.name.lowercase();
  });
  for (const auto& it : items) {
    auto row = *store_->append();
    row[cols_.tick] = false;
    row[cols_.tickable] = true;
    row[cols_.is_dir] = it.is_dir;
    row[cols_.is_parent] = false;
    row[cols_.name] = it.name;
    row[cols_.size_text] = it.size_text;
    row[cols_.uri] = it.uri;
    row[cols_.weight] = it.is_dir ? Pango::WEIGHT_BOLD : Pango::WEIGHT_NORMAL;
  }
}

void SyncPane::clear_ticks()
{
  for (auto& row : store_->children())
    row[cols_.tick] = false;
}

void SyncPane::set_list_sensitive(bool on)
{
  view_.set_sensitive(on);
  combo_.set_sensitive(on);
}

std::vector<Glib::RefPtr<Gio::File>> SyncPane::ticked() const
{
  std::vector<Glib::RefPtr<Gio::File>> out;
  for (const auto& row : store_->children()) {
    if (!row[cols_.tick] || row[cols_.is_parent])
      continue;
    const Glib::ustring uri = row[cols_.uri];
    if (!uri.empty())
      out.push_back(Gio::File::create_for_uri(uri.raw()));
  }
  return out;
}

void SyncPane::on_toggled(const Glib::ustring& path)
{
  auto it = store_->get_iter(path);
  if (!it)
    return;
  auto row = *it;
  if (!row[cols_.tickable])
    return;
  row[cols_.tick] = !row[cols_.tick];
}

void SyncPane::on_row_activated(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*)
{
  auto it = store_->get_iter(path);
  if (!it)
    return;
  auto row = *it;
  const Glib::ustring uri = row[cols_.uri];
  if (uri.empty())
    return;
  if (row[cols_.is_dir] || row[cols_.is_parent]) {
    navigate(Gio::File::create_for_uri(uri.raw()));
    return;
  }
  if (row[cols_.tickable])
    row[cols_.tick] = !row[cols_.tick];
}

SyncWindow::SyncWindow(Gtk::Window& parent, Settings& settings)
    : settings_(settings)
{
  set_transient_for(parent);
  set_title("Sync");
  set_default_size(720, 460);
  set_type_hint(Gdk::WINDOW_TYPE_HINT_DIALOG);
  set_border_width(8);

  left_.signal_status.connect(sigc::mem_fun(*this, &SyncWindow::set_status));
  right_.signal_status.connect(sigc::mem_fun(*this, &SyncWindow::set_status));
  right_.combo().signal_changed().connect(sigc::mem_fun(*this, &SyncWindow::on_device_changed));

  left_.set_hexpand(true);
  right_.set_hexpand(true);
  left_.set_size_request(160, -1);
  right_.set_size_request(160, -1);
  panes_.pack1(left_, true, true);
  panes_.pack2(right_, true, true);
  panes_.set_wide_handle(true);
  panes_.set_position(352);

  status_.set_halign(Gtk::ALIGN_START);
  status_.set_hexpand(true);
  status_.set_ellipsize(Pango::ELLIPSIZE_END);
  transfer_.signal_clicked().connect(sigc::mem_fun(*this, &SyncWindow::on_transfer));
  close_.signal_clicked().connect(sigc::mem_fun(*this, &SyncWindow::on_close));
  actions_.pack_start(status_, Gtk::PACK_EXPAND_WIDGET);
  actions_.pack_start(transfer_, Gtk::PACK_SHRINK);
  actions_.pack_start(close_, Gtk::PACK_SHRINK);

  root_.pack_start(panes_, Gtk::PACK_EXPAND_WIDGET);
  root_.pack_start(actions_, Gtk::PACK_SHRINK);
  add(root_);

  progress_conn_ = progress_.connect(sigc::mem_fun(*this, &SyncWindow::on_copy_progress));
  done_conn_ = done_.connect(sigc::mem_fun(*this, &SyncWindow::on_copy_done));

  try {
    monitor_ = Gio::VolumeMonitor::get();
    if (monitor_) {
      monitor_->signal_mount_added().connect([this](const Glib::RefPtr<Gio::Mount>&) {
        if (!busy_)
          refresh_devices();
      });
      monitor_->signal_mount_removed().connect([this](const Glib::RefPtr<Gio::Mount>&) {
        if (!busy_)
          refresh_devices();
      });
    }
  } catch (const Glib::Error&) {
  }

  add_events(Gdk::KEY_PRESS_MASK);
  signal_key_press_event().connect(sigc::mem_fun(*this, &SyncWindow::on_key_press), false);
  signal_delete_event().connect([this](GdkEventAny*) {
    hide();
    return true;
  });
  signal_hide().connect([this]() { stop_copy(); });

  left_.navigate(local_start(settings_));
  refresh_devices();
  show_all();
}

SyncWindow::~SyncWindow()
{
  progress_conn_.disconnect();
  done_conn_.disconnect();
  stop_copy();
}

void SyncWindow::set_status(const Glib::ustring& text)
{
  status_.set_text(text);
}

void SyncWindow::refresh_devices()
{
  const Glib::ustring keep = right_.combo().get_active_id();
  devices_ = list_sync_devices();

  ignore_device_ = true;
  right_.combo().remove_all();
  for (const auto& d : devices_)
    right_.combo().append(d.uri, d.label);
  ignore_device_ = false;

  if (devices_.empty()) {
    right_.set_ceiling({});
    right_.navigate({});
    right_.combo().set_active(-1);
    set_status("No USB or MTP device is mounted.");
    return;
  }

  Glib::ustring pick = keep;
  bool found = false;
  if (!pick.empty()) {
    for (const auto& d : devices_) {
      if (d.uri == pick.raw()) {
        found = true;
        break;
      }
    }
  }
  if (!found)
    pick = devices_.front().uri;
  ignore_device_ = true;
  right_.combo().set_active_id(pick);
  ignore_device_ = false;
  on_device_changed();
}

void SyncWindow::on_device_changed()
{
  if (ignore_device_)
    return;
  const Glib::ustring id = right_.combo().get_active_id();
  if (id.empty()) {
    right_.set_ceiling({});
    right_.navigate({});
    return;
  }
  auto root = Gio::File::create_for_uri(id.raw());
  right_.set_ceiling(root);
  right_.navigate(root);
  if (status_.get_text() == "No USB or MTP device is mounted.")
    set_status("");
}

bool SyncWindow::dest_writable(const Glib::RefPtr<Gio::File>& dir) const
{
  if (!dir)
    return false;
  try {
    auto info = dir->query_info("access::can-write");
    if (info && info->get_attribute_boolean("access::can-write") == false)
      return false;
  } catch (const Glib::Error&) {
  }
  return true;
}

void SyncWindow::set_busy(bool on)
{
  busy_ = on;
  left_.set_list_sensitive(!on);
  right_.set_list_sensitive(!on);
  transfer_.set_sensitive(!on);
}

void SyncWindow::on_transfer()
{
  if (busy_)
    return;
  const auto left_sel = left_.ticked();
  const auto right_sel = right_.ticked();
  if (!left_sel.empty() && !right_sel.empty()) {
    set_status("Tick files on one side only.");
    return;
  }
  if (left_sel.empty() && right_sel.empty()) {
    set_status("Tick files to copy.");
    return;
  }
  const bool to_device = !left_sel.empty();
  auto dest = to_device ? right_.current() : left_.current();
  const auto& srcs = to_device ? left_sel : right_sel;
  if (!dest) {
    set_status(to_device ? "No USB or MTP device is mounted." : "No destination folder.");
    return;
  }
  if (!dest_writable(dest)) {
    set_status("That folder is not writable. Open Internal storage on the phone.");
    return;
  }

  job_srcs_.clear();
  job_srcs_.reserve(srcs.size());
  for (const auto& f : srcs)
    job_srcs_.push_back(f->get_uri());
  job_dest_ = dest->get_uri();
  result_text_.clear();
  progress_text_ = "Copying…";
  cancellable_ = Gio::Cancellable::create();
  set_busy(true);
  set_status(progress_text_);
  copy_thread_ = std::thread([this]() { run_copy(); });
}

void SyncWindow::run_copy()
{
  int ok = 0;
  int skipped = 0;
  int failed = 0;
  bool cancelled = false;
  try {
    auto dest = Gio::File::create_for_uri(job_dest_);
    for (const auto& uri : job_srcs_) {
      if (cancellable_ && cancellable_->is_cancelled()) {
        cancelled = true;
        break;
      }
      try {
        auto src = Gio::File::create_for_uri(uri);
        const bool copied = copy_tree(src, dest, cancellable_, [this](const Glib::ustring& name) {
          {
            std::lock_guard<std::mutex> lock(mu_);
            progress_text_ = "Copying " + name;
          }
          progress_.emit();
        });
        if (copied) {
          ++ok;
        } else {
          ++skipped;
          {
            std::lock_guard<std::mutex> lock(mu_);
            progress_text_ = "Skipped " + Glib::ustring(src->get_basename()) + " (already exists)";
          }
          progress_.emit();
        }
      } catch (const Glib::Error& e) {
        if (e.code() == Gio::Error::CANCELLED) {
          cancelled = true;
          break;
        }
        if (e.code() == Gio::Error::EXISTS) {
          ++skipped;
          continue;
        }
        ++failed;
        {
          std::lock_guard<std::mutex> lock(mu_);
          progress_text_ = e.what();
        }
        progress_.emit();
      }
    }
  } catch (const Glib::Error& e) {
    if (e.code() == Gio::Error::CANCELLED)
      cancelled = true;
    else
      ++failed;
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    if (cancelled) {
      result_text_ = "Transfer cancelled.";
    } else if (failed == 0 && skipped == 0) {
      result_text_ = Glib::ustring::compose("Copied %1 item(s).", ok);
    } else if (failed == 0) {
      result_text_ = Glib::ustring::compose("Copied %1, skipped %2 (already exists).", ok, skipped);
    } else {
      result_text_ =
          Glib::ustring::compose("Copied %1, skipped %2, %3 failed.", ok, skipped, failed);
    }
  }
  done_.emit();
}

void SyncWindow::on_copy_progress()
{
  std::lock_guard<std::mutex> lock(mu_);
  if (!progress_text_.empty())
    set_status(progress_text_);
}

void SyncWindow::on_copy_done()
{
  if (copy_thread_.joinable())
    copy_thread_.join();
  set_busy(false);
  Glib::ustring text;
  {
    std::lock_guard<std::mutex> lock(mu_);
    text = result_text_;
  }
  left_.refresh();
  right_.refresh();
  left_.clear_ticks();
  right_.clear_ticks();
  set_status(text);
}

void SyncWindow::stop_copy()
{
  if (cancellable_)
    cancellable_->cancel();
  if (copy_thread_.joinable())
    copy_thread_.join();
  busy_ = false;
}

void SyncWindow::on_close()
{
  hide();
}

bool SyncWindow::on_key_press(GdkEventKey* event)
{
  if (event && event->keyval == GDK_KEY_Escape) {
    hide();
    return true;
  }
  return false;
}

}  // namespace earblaster
