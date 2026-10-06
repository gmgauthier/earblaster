/* SPDX-License-Identifier: Unlicense */

#include "player.hpp"
#include "check.hpp"

#include <gst/gst.h>

#include <cstdint>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

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

std::string write_wav()
{
  const std::string path =
      "/tmp/earblaster-player-" + std::to_string(static_cast<long long>(getpid())) + ".wav";
  std::ofstream out(path, std::ios::binary);
  const uint32_t rate = 8000;
  const uint32_t samples = rate;
  const uint32_t data_bytes = samples * 2;
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
  return path;
}

bool pump_until(const earblaster::Player& player, bool want_pending, earblaster::Player::State want)
{
  for (int i = 0; i < 400; ++i) {
    g_main_context_iteration(nullptr, false);
    if (player.chapter_seek_pending() == want_pending && player.state() == want)
      return true;
    g_usleep(10 * 1000);
  }
  return false;
}

bool pump_user_seek(const earblaster::Player& player)
{
  for (int i = 0; i < 400; ++i) {
    g_main_context_iteration(nullptr, false);
    if (!player.user_seek_pending() && player.state() == earblaster::Player::State::Playing)
      return true;
    g_usleep(10 * 1000);
  }
  return false;
}

bool near_pos(gint64 got, gint64 want)
{
  const gint64 slack = 80 * GST_MSECOND;
  return got + slack >= want && got <= want + slack;
}

}  // namespace

int main(int argc, char** argv)
{
  gst_init(&argc, &argv);
  setenv("EARBLASTER_AUDIO_SINK", "fakesink", 1);
  const std::string wav = write_wav();
  const gint64 start = GST_SECOND / 5;
  const gint64 stop = GST_SECOND / 2;

  {
    earblaster::Player player;
    CHECK(player.open(wav, start, stop));
    CHECK(player.loaded());
    CHECK(player.chapter_seek_pending());
    player.play();
    CHECK(pump_until(player, false, earblaster::Player::State::Playing));
    CHECK(!player.chapter_seek_pending());
    player.pause();
    CHECK(pump_until(player, false, earblaster::Player::State::Paused));
    CHECK(!player.chapter_seek_pending());
    player.play();
    CHECK(pump_until(player, false, earblaster::Player::State::Playing));
    CHECK(!player.chapter_seek_pending());
    player.stop();
    CHECK(player.state() == earblaster::Player::State::Stopped);
    CHECK(player.loaded());
    CHECK(player.chapter_seek_pending());
    player.play();
    CHECK(pump_until(player, false, earblaster::Player::State::Playing));
    player.stop();
  }

  {
    earblaster::Player player;
    CHECK(player.open(wav, 0, 0));
    CHECK(!player.chapter_seek_pending());
    player.play();
    CHECK(pump_until(player, false, earblaster::Player::State::Playing));
    player.stop();
    CHECK(player.state() == earblaster::Player::State::Stopped);
    CHECK(!player.chapter_seek_pending());
  }

  /* Stop leaves the pipeline NULL. A seek is kept and applied on the next play. */
  {
    earblaster::Player player;
    CHECK(player.open(wav, 0, 0));
    player.play();
    CHECK(pump_until(player, false, earblaster::Player::State::Playing));
    for (int i = 0; i < 20; ++i)
      g_main_context_iteration(nullptr, false);
    CHECK(player.duration() > 0);
    const gint64 duration = player.duration();
    player.stop();
    CHECK(player.state() == earblaster::Player::State::Stopped);
    CHECK(player.duration() == duration);
    CHECK(player.position() == 0);
    const gint64 target = GST_SECOND / 2;
    player.seek(target);
    CHECK(player.user_seek_pending());
    CHECK(player.position() == target);
    gint64 landed = -1;
    player.signal_position_changed().connect([&](gint64 pos, gint64) {
      if (landed < 0 && !player.user_seek_pending())
        landed = pos;
    });
    player.play();
    CHECK(pump_user_seek(player));
    CHECK(near_pos(landed, target));
    player.stop();
    CHECK(!player.user_seek_pending());
    CHECK(player.position() == 0);
  }

  /* A seek while stopped on a chapter starts there, not at the chapter start. */
  {
    earblaster::Player player;
    CHECK(player.open(wav, start, stop));
    player.play();
    CHECK(pump_until(player, false, earblaster::Player::State::Playing));
    player.stop();
    CHECK(player.chapter_seek_pending());
    const gint64 into = GST_SECOND / 10;
    player.seek(into);
    CHECK(player.position() == into);
    gint64 landed = -1;
    player.signal_position_changed().connect([&](gint64 pos, gint64) {
      if (landed < 0 && !player.user_seek_pending())
        landed = pos;
    });
    player.play();
    CHECK(pump_user_seek(player));
    CHECK(!player.chapter_seek_pending());
    CHECK(near_pos(landed, into));
    player.stop();
  }

  std::remove(wav.c_str());
  return suite_test::done("player");
}
