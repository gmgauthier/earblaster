/* SPDX-License-Identifier: Unlicense */

#include "settings.hpp"

#include <glib.h>
#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>

#include <cstdio>

namespace earblaster {
namespace {

std::string config_dir()
{
  return Glib::build_filename(Glib::get_user_config_dir(), "earblaster");
}

std::string config_path()
{
  return Glib::build_filename(config_dir(), "earblaster.ini");
}

}  // namespace

void Settings::load()
{
  Glib::KeyFile kf;
  try {
    kf.load_from_file(config_path());
  } catch (const Glib::Error&) {
    return;
  }
  auto get_int = [&](const char* group, const char* key, int fallback) {
    try {
      if (kf.has_key(group, key))
        return kf.get_integer(group, key);
    } catch (const Glib::Error&) {
    }
    return fallback;
  };
  auto get_dbl = [&](const char* group, const char* key, double fallback) {
    try {
      if (kf.has_key(group, key))
        return kf.get_double(group, key);
    } catch (const Glib::Error&) {
    }
    return fallback;
  };
  auto get_bool = [&](const char* group, const char* key, bool fallback) {
    try {
      if (kf.has_key(group, key))
        return kf.get_boolean(group, key);
    } catch (const Glib::Error&) {
    }
    return fallback;
  };

  window_x = get_int("window", "x", window_x);
  window_y = get_int("window", "y", window_y);
  window_w = get_int("window", "width", window_w);
  window_h = get_int("window", "height", window_h);
  volume = get_dbl("player", "volume", volume);
  shuffle = get_bool("player", "shuffle", shuffle);
  repeat = get_bool("player", "repeat", repeat);
  restore_window = get_bool("window", "restore", restore_window);
  try {
    if (kf.has_key("player", "music_dir"))
      music_dir = kf.get_string("player", "music_dir");
  } catch (const Glib::Error&) {
  }
  for (int i = 0; i < kEqBands; ++i) {
    char key[16];
    std::snprintf(key, sizeof(key), "band%d", i);
    eq[i] = get_dbl("eq", key, 0.0);
  }
}

void Settings::save() const
{
  g_mkdir_with_parents(config_dir().c_str(), 0700);
  Glib::KeyFile kf;
  kf.set_integer("window", "x", window_x);
  kf.set_integer("window", "y", window_y);
  kf.set_integer("window", "width", window_w);
  kf.set_integer("window", "height", window_h);
  kf.set_double("player", "volume", volume);
  kf.set_boolean("player", "shuffle", shuffle);
  kf.set_boolean("player", "repeat", repeat);
  kf.set_boolean("window", "restore", restore_window);
  kf.set_string("player", "music_dir", music_dir);
  for (int i = 0; i < kEqBands; ++i) {
    char key[16];
    std::snprintf(key, sizeof(key), "band%d", i);
    kf.set_double("eq", key, eq[i]);
  }
  try {
    kf.save_to_file(config_path());
  } catch (const Glib::Error&) {
  }
}

}  // namespace earblaster
