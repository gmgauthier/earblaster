/* SPDX-License-Identifier: Unlicense */

#include "cue_sheet.hpp"
#include "check.hpp"

#include <fstream>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

std::string temp_dir()
{
  const std::string dir = "/tmp/earblaster-cue-" + std::to_string(static_cast<long long>(getpid()));
  mkdir(dir.c_str(), 0700);
  return dir;
}

void write_file(const std::string& path, const std::string& body)
{
  std::ofstream out(path);
  out << body;
}

}  // namespace

int main()
{
  const std::string dir = temp_dir();
  const std::string cue = dir + "/album.cue";

  CHECK(earblaster::parse_cue_sheet(dir + "/missing.cue").empty());

  write_file(cue,
             "PERFORMER \"The Band\"\n"
             "FILE \"audio file.mp3\" MP3\n"
             "  TRACK 01 AUDIO\n"
             "    TITLE \"One\"\n"
             "    PERFORMER \"Solo\"\n"
             "    INDEX 00 00:00:00\n"
             "    INDEX 01 00:00:00\n"
             "  TRACK 02 AUDIO\n"
             "    TITLE \"Two\"\n"
             "    INDEX 01 01:00:00\n"
             "  TRACK 03 AUDIO\n"
             "    TITLE \"Skipped\"\n");
  {
    const auto files = earblaster::parse_cue_sheet(cue);
    CHECK(files.size() == 1);
    CHECK(files[0].audio_path == dir + "/audio file.mp3");
    CHECK(files[0].performer == "The Band");
    CHECK(files[0].tracks.size() == 2);
    CHECK(files[0].tracks[0].title == "One");
    CHECK(files[0].tracks[0].performer == "Solo");
    CHECK(files[0].tracks[0].start_ns == 0);
    CHECK(files[0].tracks[1].title == "Two");
    CHECK(files[0].tracks[1].performer == "The Band");
    const gint64 minute = 60LL * 75 * (1000000000LL / 75);
    CHECK(files[0].tracks[1].start_ns == minute);
    const gint64 exact = 60LL * 1000000000LL;
    CHECK(std::llabs(files[0].tracks[1].start_ns - exact) < 1000000LL);
  }

  write_file(cue,
             "FILE \"/tmp/abs.flac\" WAVE\n"
             "TRACK 01 AUDIO\n"
             "TITLE \"Abs\"\n"
             "INDEX 01 00:01:00\n");
  {
    const auto files = earblaster::parse_cue_sheet(cue);
    CHECK(files.size() == 1);
    CHECK(files[0].audio_path == "/tmp/abs.flac");
    CHECK(files[0].tracks.size() == 1);
    CHECK(files[0].tracks[0].start_ns == 75 * (1000000000LL / 75));
  }

  write_file(cue, "FILE \"a.mp3\" MP3\nTRACK 01 AUDIO\nINDEX 01 nope\n");
  CHECK(earblaster::parse_cue_sheet(cue).empty());

  std::remove(cue.c_str());
  rmdir(dir.c_str());
  return suite_test::done("cue");
}
