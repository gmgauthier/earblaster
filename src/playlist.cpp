/* SPDX-License-Identifier: Unlicense */

#include "playlist.hpp"

#include <gst/pbutils/pbutils.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>

namespace earblaster {
namespace {

namespace fs = std::filesystem;

std::string to_lower(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

std::string extension_of(const std::string& path)
{
  const auto pos = path.find_last_of('.');
  if (pos == std::string::npos || pos == path.size() - 1)
    return {};
  return to_lower(path.substr(pos + 1));
}

std::string path_to_uri(const std::string& path)
{
  if (path.find("://") != std::string::npos)
    return path;
  GError* err = nullptr;
  gchar* uri = g_filename_to_uri(path.c_str(), nullptr, &err);
  if (!uri) {
    if (err)
      g_error_free(err);
    return {};
  }
  std::string out(uri);
  g_free(uri);
  return out;
}

std::string uri_to_path(const std::string& uri)
{
  GError* err = nullptr;
  gchar* path = g_filename_from_uri(uri.c_str(), nullptr, &err);
  if (!path) {
    if (err)
      g_error_free(err);
    return uri;
  }
  std::string out(path);
  g_free(path);
  return out;
}

Glib::ustring display_title(const std::string& path)
{
  return Glib::filename_display_basename(path);
}

std::string trim(std::string s)
{
  const auto a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos)
    return {};
  const auto b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

Glib::ustring format_duration(gint64 ns)
{
  if (ns <= 0)
    return {};
  const int total = static_cast<int>(ns / (1000 * 1000 * 1000LL));
  char buf[32];
  g_snprintf(buf, sizeof(buf), "%d:%02d", total / 60, total % 60);
  return buf;
}

}  // namespace

bool Playlist::is_audio_path(const std::string& path)
{
  const auto ext = extension_of(path);
  return ext == "mp3" || ext == "ogg" || ext == "oga" || ext == "flac" ||
         ext == "wav" || ext == "m4a" || ext == "aac" || ext == "opus";
}

bool Playlist::is_m3u_path(const std::string& path)
{
  const auto ext = extension_of(path);
  return ext == "m3u" || ext == "m3u8";
}

Playlist::Playlist()
{
  store_ = Gtk::ListStore::create(columns_);
}

Playlist::~Playlist()
{
  meta_queue_.clear();
  if (discoverer_) {
    gst_discoverer_stop(discoverer_);
    gst_object_unref(discoverer_);
    discoverer_ = nullptr;
  }
}

void Playlist::clear()
{
  meta_queue_.clear();
  store_->clear();
  current_ = Gtk::TreeRowReference();
  history_.clear();
}

int Playlist::size() const
{
  return static_cast<int>(store_->children().size());
}

int Playlist::current_index() const
{
  if (!current_.is_valid())
    return -1;
  const auto path = current_.get_path();
  if (path.empty())
    return -1;
  return path[0];
}

Gtk::TreeModel::Path Playlist::current_path() const
{
  if (!current_.is_valid())
    return {};
  return current_.get_path();
}

Gtk::TreeModel::iterator Playlist::current_iter() const
{
  if (!current_.is_valid())
    return {};
  return store_->get_iter(current_.get_path());
}

std::string Playlist::iter_uri(const Gtk::TreeModel::iterator& it) const
{
  if (!it)
    return {};
  return std::string(it->get_value(columns_.uri));
}

std::string Playlist::current_uri() const
{
  return iter_uri(current_iter());
}

void Playlist::set_current(int index)
{
  if (index < 0 || index >= size()) {
    current_ = Gtk::TreeRowReference();
    return;
  }
  Gtk::TreeModel::Path path;
  path.push_back(static_cast<unsigned>(index));
  current_ = Gtk::TreeRowReference(store_, path);
}

void Playlist::set_current(const Gtk::TreeModel::Path& path)
{
  if (path.empty()) {
    current_ = Gtk::TreeRowReference();
    return;
  }
  current_ = Gtk::TreeRowReference(store_, path);
}

int Playlist::append_uri(const std::string& uri, const Glib::ustring& title)
{
  if (uri.empty())
    return 0;
  auto row = *store_->append();
  row[columns_.uri] = uri;
  row[columns_.title] = title;
  row[columns_.artist] = "";
  row[columns_.time] = "";
  row[columns_.duration_ns] = 0;
  enqueue_meta(uri);
  return 1;
}

void Playlist::enqueue_meta(const std::string& uri)
{
  if (uri.empty())
    return;
  meta_queue_.push_back(uri);
  pump_meta();
}

bool Playlist::ensure_discoverer()
{
  if (discoverer_)
    return true;
  GError* err = nullptr;
  discoverer_ = gst_discoverer_new(5 * GST_SECOND, &err);
  if (!discoverer_) {
    if (err)
      g_error_free(err);
    return false;
  }
  g_signal_connect(discoverer_, "discovered", G_CALLBACK(&Playlist::on_discovered),
                   this);
  g_signal_connect(discoverer_, "finished", G_CALLBACK(&Playlist::on_finished), this);
  gst_discoverer_start(discoverer_);
  return true;
}

void Playlist::pump_meta()
{
  if (discovering_ || meta_queue_.empty())
    return;
  if (!ensure_discoverer()) {
    meta_queue_.clear();
    return;
  }
  discovering_ = true;
  const std::string uri = meta_queue_.front();
  meta_queue_.pop_front();
  gst_discoverer_discover_uri_async(discoverer_, uri.c_str());
}

void Playlist::apply_discoverer_info(GstDiscovererInfo* info)
{
  if (!info)
    return;
  const gchar* uri = gst_discoverer_info_get_uri(info);
  if (!uri)
    return;
  const GstClockTime dur = gst_discoverer_info_get_duration(info);
  const GstTagList* tags = gst_discoverer_info_get_tags(info);
  gchar* title = nullptr;
  gchar* artist = nullptr;
  if (tags) {
    gst_tag_list_get_string(tags, GST_TAG_TITLE, &title);
    gst_tag_list_get_string(tags, GST_TAG_ARTIST, &artist);
  }
  for (auto& row : store_->children()) {
    if (std::string(row.get_value(columns_.uri)) != uri)
      continue;
    if (title && *title)
      row[columns_.title] = title;
    if (artist && *artist)
      row[columns_.artist] = artist;
    if (dur != 0 && dur != GST_CLOCK_TIME_NONE) {
      row[columns_.duration_ns] = static_cast<gint64>(dur);
      row[columns_.time] = format_duration(static_cast<gint64>(dur));
    }
    break;
  }
  g_free(title);
  g_free(artist);
}

void Playlist::on_discovered(GstDiscoverer*, GstDiscovererInfo* info, GError*,
                            gpointer self)
{
  static_cast<Playlist*>(self)->apply_discoverer_info(info);
}

void Playlist::on_finished(GstDiscoverer*, gpointer self)
{
  auto* p = static_cast<Playlist*>(self);
  p->discovering_ = false;
  p->pump_meta();
}

int Playlist::add_audio_file(const std::string& path)
{
  if (!is_audio_path(path))
    return 0;
  const std::string uri = path_to_uri(path);
  if (uri.empty())
    return 0;
  return append_uri(uri, display_title(path));
}

int Playlist::add_files(const std::vector<std::string>& paths)
{
  std::vector<std::string> audio;
  audio.reserve(paths.size());
  for (const auto& p : paths) {
    if (is_audio_path(p))
      audio.push_back(p);
  }
  std::sort(audio.begin(), audio.end());
  int n = 0;
  for (const auto& p : audio)
    n += add_audio_file(p);
  return n;
}

int Playlist::add_folder(const std::string& dir)
{
  std::error_code ec;
  if (!fs::is_directory(dir, ec))
    return 0;
  std::vector<std::string> audio;
  const auto opts = fs::directory_options::skip_permission_denied;
  /* depth 0 = selected folder, depth 1 = album subfolder. Do not go deeper. */
  for (auto it = fs::recursive_directory_iterator(dir, opts, ec);
       it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    const std::string name = it->path().filename().string();
    if (!name.empty() && name[0] == '.') {
      if (it->is_directory(ec))
        it.disable_recursion_pending();
      continue;
    }
    if (it->is_directory(ec)) {
      if (it.depth() >= 1)
        it.disable_recursion_pending();
      continue;
    }
    if (it.depth() > 1 || !it->is_regular_file(ec))
      continue;
    const std::string p = it->path().string();
    if (is_audio_path(p))
      audio.push_back(p);
  }
  std::sort(audio.begin(), audio.end());
  int n = 0;
  for (const auto& p : audio)
    n += add_audio_file(p);
  return n;
}

int Playlist::add_m3u(const std::string& path)
{
  std::ifstream in(path);
  if (!in)
    return 0;
  const fs::path base = fs::path(path).parent_path();
  int n = 0;
  std::string line;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#')
      continue;
    fs::path item(line);
    if (item.is_relative())
      item = base / item;
    n += add_audio_file(item.string());
  }
  return n;
}

int Playlist::add_dropped(const std::vector<Glib::ustring>& uris)
{
  int n = 0;
  std::vector<std::string> files;
  for (const auto& uri : uris) {
    const std::string path = uri_to_path(std::string(uri));
    std::error_code ec;
    if (fs::is_directory(path, ec))
      n += add_folder(path);
    else if (is_m3u_path(path))
      n += add_m3u(path);
    else if (is_audio_path(path))
      files.push_back(path);
  }
  n += add_files(files);
  return n;
}

bool Playlist::save_m3u(const std::string& path) const
{
  std::ofstream out(path);
  if (!out)
    return false;
  out << "#EXTM3U\n";
  for (const auto& row : store_->children()) {
    const std::string uri = std::string(row.get_value(columns_.uri));
    out << uri_to_path(uri) << '\n';
  }
  return static_cast<bool>(out);
}

bool Playlist::next()
{
  const int n = size();
  if (n <= 0)
    return false;
  const int cur = current_index();
  if (shuffle_ && n > 1) {
    if (cur >= 0)
      history_.push_back(cur);
    std::uniform_int_distribution<int> dist(0, n - 1);
    int pick = dist(rng_);
    if (pick == cur)
      pick = (pick + 1) % n;
    set_current(pick);
    return true;
  }
  int nxt = (cur < 0) ? 0 : cur + 1;
  if (nxt >= n) {
    if (repeat_ == Repeat::All) {
      nxt = 0;
    } else {
      return false;
    }
  }
  set_current(nxt);
  return true;
}

bool Playlist::prev()
{
  const int n = size();
  if (n <= 0)
    return false;
  if (shuffle_ && !history_.empty()) {
    const int idx = history_.back();
    history_.pop_back();
    if (idx >= 0 && idx < n) {
      set_current(idx);
      return true;
    }
  }
  const int cur = current_index();
  int prv = (cur < 0) ? 0 : cur - 1;
  if (prv < 0) {
    if (repeat_ == Repeat::All) {
      prv = n - 1;
    } else {
      return false;
    }
  }
  set_current(prv);
  return true;
}

void Playlist::set_shuffle(bool shuffle)
{
  shuffle_ = shuffle;
  if (!shuffle_)
    history_.clear();
}

void Playlist::set_repeat(Repeat repeat)
{
  repeat_ = repeat;
}

void Playlist::update_current_meta(const Glib::ustring& title,
                                   const Glib::ustring& artist, gint64 duration_ns)
{
  auto it = current_iter();
  if (!it)
    return;
  if (!title.empty())
    it->set_value(columns_.title, title);
  if (!artist.empty())
    it->set_value(columns_.artist, artist);
  if (duration_ns > 0) {
    it->set_value(columns_.duration_ns, duration_ns);
    it->set_value(columns_.time, format_duration(duration_ns));
  }
}

bool Playlist::remove_paths(const std::vector<Gtk::TreeModel::Path>& paths)
{
  bool killed_current = false;
  const auto cur = current_path();
  auto ordered = paths;
  std::sort(ordered.rbegin(), ordered.rend());
  for (const auto& path : ordered) {
    if (!cur.empty() && path == cur)
      killed_current = true;
    auto it = store_->get_iter(path);
    if (it)
      store_->erase(it);
  }
  if (killed_current)
    current_ = Gtk::TreeRowReference();
  return killed_current;
}

}  // namespace earblaster
