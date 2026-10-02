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

  return suite_test::done("playlist");
}
