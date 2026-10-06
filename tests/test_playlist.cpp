/* SPDX-License-Identifier: Unlicense */

#include "playlist.hpp"
#include "check.hpp"

#include <gst/gst.h>
#include <gtkmm.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

namespace fs = std::filesystem;

void write_u16(std::ostream& out, uint16_t v)
{
  const char b[2] = {static_cast<char>(v & 0xff), static_cast<char>((v >> 8) & 0xff)};
  out.write(b, 2);
}

void write_u32(std::ostream& out, uint32_t v)
{
  const char b[4] = {static_cast<char>(v & 0xff), static_cast<char>((v >> 8) & 0xff),
                     static_cast<char>((v >> 16) & 0xff), static_cast<char>((v >> 24) & 0xff)};
  out.write(b, 4);
}

void write_wav(const std::string& path)
{
  std::ofstream out(path, std::ios::binary);
  const uint32_t rate = 8000;
  const uint32_t data_bytes = rate * 2;
  out.write("RIFF", 4);
  write_u32(out, 36 + data_bytes);
  out.write("WAVE", 4);
  out.write("fmt ", 4);
  write_u32(out, 16);
  write_u16(out, 1);
  write_u16(out, 1);
  write_u32(out, rate);
  write_u32(out, rate * 2);
  write_u16(out, 2);
  write_u16(out, 16);
  out.write("data", 4);
  write_u32(out, data_bytes);
  const std::string silence(data_bytes, '\0');
  out.write(silence.data(), static_cast<std::streamsize>(silence.size()));
}

struct Fixture {
  fs::path dir;
  std::vector<std::string> files;

  explicit Fixture(int n)
  {
    dir = fs::temp_directory_path() /
          ("earblaster-playlist-" + std::to_string(static_cast<long long>(getpid())));
    fs::create_directories(dir);
    for (int i = 0; i < n; ++i) {
      const std::string p = (dir / ("track" + std::to_string(i + 1) + ".wav")).string();
      write_wav(p);
      files.push_back(p);
    }
  }
  ~Fixture()
  {
    std::error_code ec;
    fs::remove_all(dir, ec);
  }
};

std::string row_uri(earblaster::Playlist& pl, int index)
{
  Gtk::TreeModel::Path path;
  path.push_back(static_cast<unsigned>(index));
  auto it = pl.store()->get_iter(path);
  if (!it)
    return {};
  return std::string(it->get_value(pl.columns().uri));
}

std::vector<Gtk::TreeModel::Path> rows(std::initializer_list<int> idx)
{
  std::vector<Gtk::TreeModel::Path> out;
  for (int i : idx) {
    Gtk::TreeModel::Path p;
    p.push_back(static_cast<unsigned>(i));
    out.push_back(p);
  }
  return out;
}

void test_remove_playing_row_continues_with_successor(const Fixture& fx)
{
  /* Play track 3 of 5, delete it: Next plays what was track 4. */
  earblaster::Playlist pl;
  CHECK(pl.add_files(fx.files) == 5);
  pl.set_current(2);
  const std::string track4 = row_uri(pl, 3);
  CHECK(pl.remove_paths(rows({2})));
  CHECK(pl.size() == 4);
  CHECK(pl.next());
  CHECK(pl.current_uri() == track4);
  CHECK(pl.current_index() == 2);
}

void test_remove_playing_row_with_rows_above(const Fixture& fx)
{
  /* Removing rows above the playing row as well still lands on its successor. */
  earblaster::Playlist pl;
  CHECK(pl.add_files(fx.files) == 5);
  pl.set_current(2);
  const std::string track4 = row_uri(pl, 3);
  CHECK(pl.remove_paths(rows({0, 2})));
  CHECK(pl.next());
  CHECK(pl.current_uri() == track4);
}

void test_remove_playing_row_with_shuffle(const Fixture& fx)
{
  /* Shuffle on: the successor still follows a removed playing row. */
  for (int round = 0; round < 20; ++round) {
    earblaster::Playlist pl;
    CHECK(pl.add_files(fx.files) == 5);
    pl.set_shuffle(true);
    pl.set_current(1);
    const std::string track3 = row_uri(pl, 2);
    CHECK(pl.remove_paths(rows({1})));
    CHECK(pl.next());
    CHECK(pl.current_uri() == track3);
  }
}

void test_remove_last_playing_row_stops(const Fixture& fx)
{
  earblaster::Playlist pl;
  CHECK(pl.add_files(fx.files) == 5);
  pl.set_current(4);
  CHECK(pl.remove_paths(rows({4})));
  CHECK(!pl.next());

  earblaster::Playlist wrap;
  CHECK(wrap.add_files(fx.files) == 5);
  wrap.set_repeat(earblaster::Playlist::Repeat::All);
  wrap.set_current(4);
  const std::string first = row_uri(wrap, 0);
  CHECK(wrap.remove_paths(rows({4})));
  CHECK(wrap.next());
  CHECK(wrap.current_uri() == first);
}

void test_remove_other_row_keeps_current(const Fixture& fx)
{
  earblaster::Playlist pl;
  CHECK(pl.add_files(fx.files) == 5);
  pl.set_current(2);
  const std::string track3 = row_uri(pl, 2);
  const std::string track4 = row_uri(pl, 3);
  CHECK(!pl.remove_paths(rows({0})));
  CHECK(pl.current_uri() == track3);
  CHECK(pl.next());
  CHECK(pl.current_uri() == track4);
}

void test_shuffle_history_survives_removal_above(const Fixture& fx)
{
  /* Shuffle: remember row 4, delete row 1 above it, Previous returns to the same track. */
  earblaster::Playlist pl;
  CHECK(pl.add_files(fx.files) == 5);
  pl.set_shuffle(true);
  pl.set_current(3);
  const std::string remembered = row_uri(pl, 3);
  CHECK(pl.next());
  const std::string after = pl.current_uri();
  CHECK(after != remembered);
  int del = 0;
  if (row_uri(pl, del) == after)
    del = 1;
  CHECK(!pl.remove_paths(rows({del})));
  CHECK(pl.prev());
  CHECK(pl.current_uri() == remembered);
}

void test_shuffle_history_survives_reorder(const Fixture& fx)
{
  /* Shuffle: remember row 2, move it to the top, Previous returns to the same track. */
  earblaster::Playlist pl;
  CHECK(pl.add_files(fx.files) == 5);
  pl.set_shuffle(true);
  pl.set_current(2);
  const std::string remembered = row_uri(pl, 2);
  CHECK(pl.next());
  Gtk::TreeModel::Path from;
  from.push_back(2);
  Gtk::TreeModel::Path top;
  top.push_back(0);
  pl.store()->move(pl.store()->get_iter(from), pl.store()->get_iter(top));
  CHECK(row_uri(pl, 0) == remembered);
  CHECK(pl.prev());
  CHECK(pl.current_uri() == remembered);
}

void test_shuffle_history_skips_removed_row(const Fixture& fx)
{
  /* The remembered row itself is deleted: Previous does not land on its stale index. */
  earblaster::Playlist pl;
  CHECK(pl.add_files(fx.files) == 5);
  pl.set_shuffle(true);
  pl.set_current(4);
  const std::string remembered = row_uri(pl, 4);
  CHECK(pl.next());
  const int cur = pl.current_index();
  CHECK(!pl.remove_paths(rows({4})));
  const std::string before = pl.current_uri();
  const bool moved = pl.prev();
  CHECK(pl.current_uri() != remembered);
  /* With no usable history Previous walks back one row from the current one. */
  if (cur > 0) {
    CHECK(moved);
    CHECK(pl.current_index() == cur - 1);
  } else {
    CHECK(!moved);
    CHECK(pl.current_uri() == before);
  }
}

void test_cue_plus_its_audio_adds_only_chapters(const Fixture& fx)
{
  /* `earblaster album.cue album.wav`: one row per chapter, no extra full-file row.
   * MainWindow::open_paths hands the whole selection to add_files for this. */
  const std::string cue = (fx.dir / "album.cue").string();
  {
    std::ofstream out(cue);
    out << "FILE \"track1.wav\" WAVE\n"
        << "  TRACK 01 AUDIO\n    TITLE \"One\"\n    INDEX 01 00:00:00\n"
        << "  TRACK 02 AUDIO\n    TITLE \"Two\"\n    INDEX 01 00:00:40\n";
  }
  earblaster::Playlist pl;
  CHECK(pl.add_files({cue, fx.files[0]}) == 2);
  CHECK(pl.size() == 2);
  for (int i = 0; i < pl.size(); ++i) {
    Gtk::TreeModel::Path p;
    p.push_back(static_cast<unsigned>(i));
    CHECK(pl.store()->get_iter(p)->get_value(pl.columns().cue));
  }

  /* Audio first on the command line makes no difference. */
  earblaster::Playlist rev;
  CHECK(rev.add_files({fx.files[0], cue}) == 2);

  /* Other files in the same selection are still added. */
  earblaster::Playlist mix;
  CHECK(mix.add_files({cue, fx.files[0], fx.files[1]}) == 3);

  /* The audio spelled with a `..` detour is still the claimed file. */
  earblaster::Playlist detour;
  const std::string spelled =
      (fx.dir / ".." / fx.dir.filename() / "track1.wav").string();
  CHECK(detour.add_files({cue, spelled}) == 2);
  std::remove(cue.c_str());
}

struct RowData {
  std::string uri;
  std::string title;
  std::string artist;
  gint64 start_ns;
  gint64 stop_ns;
  bool cue;
};

std::vector<RowData> dump(earblaster::Playlist& pl)
{
  std::vector<RowData> out;
  const auto& c = pl.columns();
  for (const auto& row : pl.store()->children())
    out.push_back({std::string(row.get_value(c.uri)), std::string(row.get_value(c.title)),
                   std::string(row.get_value(c.artist)), row.get_value(c.start_ns),
                   row.get_value(c.stop_ns), row.get_value(c.cue)});
  return out;
}

void test_save_playlist_keeps_cue_ranges(const Fixture& fx)
{
  /* Save Playlist, reload: chapter rows keep their range, title, and artist. */
  const std::string cue = (fx.dir / "ranges.cue").string();
  {
    std::ofstream out(cue);
    out << "PERFORMER \"Band\"\n"
        << "FILE \"track2.wav\" WAVE\n"
        << "  TRACK 01 AUDIO\n    TITLE \"Intro\"\n    INDEX 01 00:00:00\n"
        << "  TRACK 02 AUDIO\n    TITLE \"Middle - Part\"\n    INDEX 01 00:00:20\n"
        << "  TRACK 03 AUDIO\n    TITLE \"End\"\n    INDEX 01 00:00:50\n";
  }
  earblaster::Playlist pl;
  CHECK(pl.add_cue(cue) == 3);
  CHECK(pl.add_audio_file(fx.files[2]) == 1);
  const auto before = dump(pl);
  CHECK(before[1].start_ns > 0);

  const std::string m3u = (fx.dir / "saved.m3u").string();
  CHECK(pl.save_m3u(m3u));

  earblaster::Playlist back;
  CHECK(back.add_m3u(m3u) == 4);
  const auto after = dump(back);
  CHECK(after.size() == before.size());
  for (size_t i = 0; i < before.size() && i < after.size(); ++i) {
    CHECK(after[i].uri == before[i].uri);
    CHECK(after[i].start_ns == before[i].start_ns);
    CHECK(after[i].stop_ns == before[i].stop_ns);
    CHECK(after[i].cue == before[i].cue);
    if (before[i].cue) {
      CHECK(after[i].title == before[i].title);
      CHECK(after[i].artist == before[i].artist);
    }
  }
  std::remove(cue.c_str());
  std::remove(m3u.c_str());
}

void test_plain_m3u_still_loads(const Fixture& fx)
{
  const std::string m3u = (fx.dir / "plain.m3u").string();
  {
    std::ofstream out(m3u);
    out << "#EXTM3U\n#EXTINF:1,Whatever\ntrack1.wav\ntrack2.wav\n";
  }
  earblaster::Playlist pl;
  CHECK(pl.add_m3u(m3u) == 2);
  const auto rows = dump(pl);
  for (const auto& r : rows) {
    CHECK(!r.cue);
    CHECK(r.start_ns == 0);
    CHECK(r.stop_ns == 0);
  }
  std::remove(m3u.c_str());
}

void test_dead_m3u_does_not_load_folder(const Fixture& fx)
{
  /* The folder has audio. An m3u with nothing playable must stay empty. */
  const std::string empty_m3u = (fx.dir / "empty.m3u").string();
  {
    std::ofstream out(empty_m3u);
    out << "#EXTM3U\n";
  }
  earblaster::Playlist empty;
  CHECK(empty.add_m3u(empty_m3u) == 0);
  CHECK(empty.empty());

  const std::string missing = (fx.dir / "missing.m3u").string();
  {
    std::ofstream out(missing);
    out << "no-such-track.flac\n";
  }
  earblaster::Playlist gone;
  CHECK(gone.add_m3u(missing) == 0);
  CHECK(gone.empty());

  std::remove(empty_m3u.c_str());
  std::remove(missing.c_str());
}

}  // namespace

int main(int argc, char** argv)
{
  gst_init(&argc, &argv);
  Gtk::Main::init_gtkmm_internals();
  Fixture fx(5);

  test_remove_playing_row_continues_with_successor(fx);
  test_remove_playing_row_with_rows_above(fx);
  test_remove_playing_row_with_shuffle(fx);
  test_remove_last_playing_row_stops(fx);
  test_remove_other_row_keeps_current(fx);
  test_shuffle_history_survives_removal_above(fx);
  test_shuffle_history_survives_reorder(fx);
  test_shuffle_history_skips_removed_row(fx);
  test_cue_plus_its_audio_adds_only_chapters(fx);
  test_save_playlist_keeps_cue_ranges(fx);
  test_plain_m3u_still_loads(fx);
  test_dead_m3u_does_not_load_folder(fx);

  return suite_test::done("playlist");
}
