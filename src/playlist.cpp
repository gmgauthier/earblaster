/* SPDX-License-Identifier: Unlicense */

#include "playlist.hpp"
#include "cover_art.hpp"
#include "cue_sheet.hpp"

#include <gst/pbutils/pbutils.h>
#include <taglib/fileref.h>
#include <taglib/audioproperties.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>

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
  const int total = static_cast<int>((ns + 500000000LL) / (1000 * 1000 * 1000LL));
  char buf[32];
  g_snprintf(buf, sizeof(buf), "%d:%02d", total / 60, total % 60);
  return buf;
}

const char* kAltExt[] = {"ogg", "oga", "mp3", "flac", "wav", "m4a", "m4b", "aac", "opus", nullptr};

std::string leading_track(const std::string& name)
{
  size_t i = 0;
  while (i < name.size() && std::isdigit(static_cast<unsigned char>(name[i])))
    ++i;
  if (i == 0)
    return {};
  return name.substr(0, i);
}

/* Map an M3U line to a file that exists: exact path, same stem with another
 * audio extension, or a unique leading-track-number match in the same folder. */
std::string resolve_existing_audio(fs::path item)
{
  std::error_code ec;
  if (fs::is_regular_file(item, ec) && Playlist::is_audio_path(item.string()))
    return item.string();

  const fs::path dir = item.parent_path();
  const std::string stem = item.stem().string();
  if (!dir.empty() && !stem.empty()) {
    for (int i = 0; kAltExt[i]; ++i) {
      const fs::path cand = dir / (stem + "." + kAltExt[i]);
      if (fs::is_regular_file(cand, ec))
        return cand.string();
    }
  }

  const std::string prefix = leading_track(item.filename().string());
  if (prefix.empty() || dir.empty())
    return {};
  std::vector<fs::path> hits;
  fs::directory_iterator it(dir, ec);
  if (ec)
    return {};
  for (; it != fs::directory_iterator(); it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (!it->is_regular_file(ec))
      continue;
    const fs::path p = it->path();
    if (!Playlist::is_audio_path(p.string()))
      continue;
    const std::string name = p.filename().string();
    if (name.size() <= prefix.size() || name.compare(0, prefix.size(), prefix) != 0)
      continue;
    if (std::isdigit(static_cast<unsigned char>(name[prefix.size()])))
      continue;
    hits.push_back(p);
  }
  if (hits.size() == 1)
    return hits[0].string();
  return {};
}

/* Same file spelled differently (relative, `..`, symlinked dir) compares equal. */
std::string canonical_or_same(const std::string& path)
{
  std::error_code ec;
  const fs::path c = fs::weakly_canonical(fs::absolute(path, ec), ec);
  return ec ? path : c.string();
}

}  // namespace

bool Playlist::is_audio_path(const std::string& path)
{
  const auto ext = extension_of(path);
  return ext == "mp3" || ext == "ogg" || ext == "oga" || ext == "flac" || ext == "wav" ||
         ext == "m4a" || ext == "m4b" || ext == "aac" || ext == "opus";
}

bool Playlist::is_m3u_path(const std::string& path)
{
  const auto ext = extension_of(path);
  return ext == "m3u" || ext == "m3u8";
}

bool Playlist::is_cue_path(const std::string& path)
{
  return extension_of(path) == "cue";
}

Playlist::Playlist()
{
  store_ = Gtk::ListStore::create(columns_);
}

Playlist::~Playlist()
{
  meta_idle_.disconnect();
  cover_idle_.disconnect();
  meta_queue_.clear();
  cover_queue_.clear();
  if (discoverer_) {
    gst_discoverer_stop(discoverer_);
    gst_object_unref(discoverer_);
    discoverer_ = nullptr;
  }
}

void Playlist::clear()
{
  meta_idle_.disconnect();
  cover_idle_.disconnect();
  meta_queue_.clear();
  cover_queue_.clear();
  discovering_ = false;
  store_->clear();
  current_ = Gtk::TreeRowReference();
  history_.clear();
  successor_ = -1;
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

int Playlist::index_of_id(gint64 id) const
{
  int i = 0;
  for (const auto& row : store_->children()) {
    if (row.get_value(columns_.id) == id)
      return i;
    ++i;
  }
  return -1;
}

std::string Playlist::current_uri() const
{
  return iter_uri(current_iter());
}

gint64 Playlist::current_start_ns() const
{
  auto it = current_iter();
  if (!it)
    return 0;
  return it->get_value(columns_.start_ns);
}

gint64 Playlist::current_stop_ns() const
{
  auto it = current_iter();
  if (!it)
    return 0;
  return it->get_value(columns_.stop_ns);
}

void Playlist::set_current(int index)
{
  successor_ = -1;
  if (index < 0 || index >= size()) {
    current_ = Gtk::TreeRowReference();
    return;
  }
  Gtk::TreeModel::Path path;
  path.push_back(static_cast<unsigned>(index));
  current_ = Gtk::TreeRowReference(store_, path);
  schedule_meta();
}

void Playlist::set_current(const Gtk::TreeModel::Path& path)
{
  successor_ = -1;
  if (path.empty()) {
    current_ = Gtk::TreeRowReference();
    return;
  }
  current_ = Gtk::TreeRowReference(store_, path);
  schedule_meta();
}

int Playlist::append_uri(const std::string& uri, const Glib::ustring& title, gint64 start_ns,
                         gint64 stop_ns, bool cue, const Glib::ustring& artist)
{
  if (uri.empty())
    return 0;
  auto row = *store_->append();
  row[columns_.uri] = uri;
  row[columns_.title] = title;
  row[columns_.artist] = artist;
  row[columns_.time] = "";
  row[columns_.duration_ns] = 0;
  row[columns_.start_ns] = start_ns;
  row[columns_.stop_ns] = stop_ns;
  row[columns_.cue] = cue;
  row[columns_.id] = next_id_++;
  if (stop_ns > start_ns && start_ns >= 0) {
    row[columns_.duration_ns] = stop_ns - start_ns;
    row[columns_.time] = format_duration(stop_ns - start_ns);
  }
  enqueue_meta(uri);
  enqueue_cover(uri);
  return 1;
}

void Playlist::enqueue_meta(const std::string& uri)
{
  if (uri.empty())
    return;
  meta_queue_.push_back(uri);
  schedule_meta();
}

void Playlist::enqueue_cover(const std::string& uri)
{
  if (uri.empty())
    return;
  cover_queue_.push_back(uri);
  schedule_cover();
}

void Playlist::schedule_cover()
{
  if (cover_idle_.connected())
    return;
  cover_idle_ = Glib::signal_idle().connect([this]() {
    pump_cover();
    return !cover_queue_.empty();
  });
}

void Playlist::pump_cover()
{
  if (cover_queue_.empty())
    return;
  const std::string uri = cover_queue_.front();
  cover_queue_.pop_front();
  Glib::RefPtr<Gdk::Pixbuf> pix;
  auto cached = cover_cache_.find(uri);
  if (cached != cover_cache_.end()) {
    pix = cached->second;
  } else {
    pix = load_cover_thumb(uri, 32);
    cover_cache_[uri] = pix;
  }
  if (!pix)
    return;
  for (auto& row : store_->children()) {
    if (std::string(row.get_value(columns_.uri)) == uri)
      row[columns_.cover] = pix;
  }
}

void Playlist::schedule_meta()
{
  if (meta_idle_.connected())
    return;
  meta_idle_ = Glib::signal_idle().connect([this]() {
    pump_meta();
    return false;
  });
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
  g_signal_connect(discoverer_, "discovered", G_CALLBACK(&Playlist::on_discovered), this);
  g_signal_connect(discoverer_, "finished", G_CALLBACK(&Playlist::on_finished), this);
  gst_discoverer_start(discoverer_);
  return true;
}

void Playlist::pump_meta()
{
  if (discovering_ || meta_queue_.empty())
    return;
  /* Do not discover the URI playbin is using. Opening the same file twice
   * (especially a large track on SMB/CIFS) stalls playback. */
  const std::string playing = current_uri();
  std::string uri;
  if (!playing.empty()) {
    auto it = std::find_if(meta_queue_.begin(), meta_queue_.end(),
                           [&](const std::string& u) { return u != playing; });
    if (it == meta_queue_.end())
      return;
    uri = *it;
    meta_queue_.erase(it);
  } else {
    uri = meta_queue_.front();
    meta_queue_.pop_front();
  }
  if (!ensure_discoverer()) {
    meta_queue_.clear();
    return;
  }
  discovering_ = true;
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
  const gint64 file_dur = (dur != 0 && dur != GST_CLOCK_TIME_NONE) ? static_cast<gint64>(dur) : 0;
  for (auto& row : store_->children()) {
    if (std::string(row.get_value(columns_.uri)) != uri)
      continue;
    const bool cue = row.get_value(columns_.cue);
    if (!cue) {
      if (title && *title)
        row[columns_.title] = title;
      if (artist && *artist)
        row[columns_.artist] = artist;
    }
    gint64 start = row.get_value(columns_.start_ns);
    gint64 stop = row.get_value(columns_.stop_ns);
    if (cue && stop <= start && file_dur > start) {
      stop = file_dur;
      row[columns_.stop_ns] = stop;
    }
    if (cue && stop > start) {
      row[columns_.duration_ns] = stop - start;
      row[columns_.time] = format_duration(stop - start);
    } else if (!cue && file_dur > 0) {
      row[columns_.duration_ns] = file_dur;
      row[columns_.time] = format_duration(file_dur);
    }
  }
  g_free(title);
  g_free(artist);
}

void Playlist::on_discovered(GstDiscoverer*, GstDiscovererInfo* info, GError*, gpointer self)
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

gint64 taglib_duration_ns(const std::string& path)
{
  TagLib::FileRef ref(path.c_str(), true);
  if (ref.isNull() || !ref.audioProperties())
    return 0;
  const int ms = ref.audioProperties()->lengthInMilliseconds();
  if (ms <= 0)
    return 0;
  return static_cast<gint64>(ms) * 1000000LL;
}

int Playlist::add_cue(const std::string& path)
{
  if (!is_cue_path(path))
    return 0;
  const auto files = parse_cue_sheet(path);
  int n = 0;
  for (const auto& f : files) {
    const std::string audio = resolve_existing_audio(fs::path(f.audio_path));
    if (audio.empty())
      continue;
    const std::string uri = path_to_uri(audio);
    if (uri.empty())
      continue;
    const gint64 file_dur = taglib_duration_ns(audio);
    for (size_t i = 0; i < f.tracks.size(); ++i) {
      const auto& t = f.tracks[i];
      if (t.start_ns < 0)
        continue;
      gint64 stop = 0;
      if (i + 1 < f.tracks.size() && f.tracks[i + 1].start_ns > t.start_ns)
        stop = f.tracks[i + 1].start_ns;
      else if (file_dur > t.start_ns)
        stop = file_dur;
      Glib::ustring title = t.title;
      if (title.empty())
        title = display_title(audio);
      Glib::ustring artist = t.performer;
      if (artist.empty())
        artist = f.performer;
      n += append_uri(uri, title, t.start_ns, stop, true, artist);
    }
  }
  return n;
}

int Playlist::add_files(const std::vector<std::string>& paths)
{
  std::vector<std::string> audio;
  std::vector<std::string> cues;
  std::vector<std::string> lists;
  audio.reserve(paths.size());
  for (const auto& p : paths) {
    if (is_cue_path(p))
      cues.push_back(p);
    else if (is_m3u_path(p))
      lists.push_back(p);
    else if (is_audio_path(p))
      audio.push_back(p);
  }
  std::sort(cues.begin(), cues.end());
  std::sort(audio.begin(), audio.end());
  int n = 0;
  std::set<std::string> cue_audio;
  for (const auto& c : cues) {
    for (const auto& f : parse_cue_sheet(c)) {
      const std::string a = resolve_existing_audio(fs::path(f.audio_path));
      if (!a.empty())
        cue_audio.insert(canonical_or_same(a));
    }
    n += add_cue(c);
  }
  for (const auto& m : lists)
    n += add_m3u(m);
  for (const auto& p : audio) {
    if (cue_audio.count(canonical_or_same(p)))
      continue;
    n += add_audio_file(p);
  }
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
    if (is_cue_path(p) || is_audio_path(p) || is_m3u_path(p))
      audio.push_back(p);
  }
  std::sort(audio.begin(), audio.end());
  return add_files(audio);
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
    if (line.size() >= 2 && ((line.front() == '"' && line.back() == '"') ||
                             (line.front() == '\'' && line.back() == '\'')))
      line = trim(line.substr(1, line.size() - 2));
    if (line.compare(0, 7, "http://") == 0 || line.compare(0, 8, "https://") == 0)
      continue;
    if (line.find("://") != std::string::npos) {
      GError* err = nullptr;
      gchar* local = g_filename_from_uri(line.c_str(), nullptr, &err);
      if (!local) {
        if (err)
          g_error_free(err);
        continue;
      }
      line = local;
      g_free(local);
    }
    fs::path item(line);
    if (item.is_relative())
      item = base / item;
    std::error_code ec;
    const fs::path canon = fs::weakly_canonical(item, ec);
    if (!ec)
      item = canon;
    const std::string resolved = resolve_existing_audio(item);
    if (resolved.empty())
      continue;
    n += add_audio_file(resolved);
  }
  if (n == 0 && !base.empty())
    n += add_folder(base.string());
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
    else if (is_m3u_path(path) || is_cue_path(path) || is_audio_path(path))
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
  if (cur < 0 && successor_ >= 0) {
    /* The playing row was removed: continue with the row that followed it. */
    int nxt = successor_;
    if (nxt >= n) {
      if (repeat_ != Repeat::All) {
        successor_ = -1;
        return false;
      }
      nxt = 0;
    }
    set_current(nxt);
    return true;
  }
  if (shuffle_ && n > 1) {
    if (auto it = current_iter())
      history_.push_back(it->get_value(columns_.id));
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
  if (shuffle_) {
    /* History holds row ids, so deletes and reorders cannot point it at a
     * different row. Ids of rows that are gone are skipped. */
    const int cur_now = current_index();
    while (!history_.empty()) {
      const gint64 id = history_.back();
      history_.pop_back();
      const int idx = index_of_id(id);
      if (idx >= 0 && idx != cur_now) {
        set_current(idx);
        return true;
      }
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

void Playlist::update_current_meta(const Glib::ustring& title, const Glib::ustring& artist,
                                   gint64 duration_ns)
{
  auto it = current_iter();
  if (!it)
    return;
  const bool cue = it->get_value(columns_.cue);
  if (!cue) {
    if (!title.empty())
      it->set_value(columns_.title, title);
    if (!artist.empty())
      it->set_value(columns_.artist, artist);
  }
  if (duration_ns > 0) {
    it->set_value(columns_.duration_ns, duration_ns);
    it->set_value(columns_.time, format_duration(duration_ns));
  }
}

bool Playlist::remove_paths(const std::vector<Gtk::TreeModel::Path>& paths)
{
  bool killed_current = false;
  const auto cur = current_path();
  const int cur_index = current_index();
  int removed_before = 0;
  auto ordered = paths;
  std::sort(ordered.rbegin(), ordered.rend());
  ordered.erase(std::unique(ordered.begin(), ordered.end()), ordered.end());
  for (const auto& path : ordered) {
    if (!cur.empty() && path == cur)
      killed_current = true;
    else if (cur_index >= 0 && !path.empty() && static_cast<int>(path[0]) < cur_index &&
             store_->get_iter(path))
      ++removed_before;
    auto it = store_->get_iter(path);
    if (it)
      store_->erase(it);
  }
  if (killed_current) {
    current_ = Gtk::TreeRowReference();
    /* Rows after the removed one keep their order; the first survivor sits
     * at the old index minus the rows removed above it. */
    successor_ = cur_index - removed_before;
  }
  return killed_current;
}

}  // namespace earblaster
