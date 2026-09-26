/* SPDX-License-Identifier: Unlicense */

#include "cue_sheet.hpp"

#include <glibmm/miscutils.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <string>

namespace earblaster {
namespace {

std::string trim(std::string s)
{
  const auto a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos)
    return {};
  const auto b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

std::string unquote(std::string s)
{
  s = trim(s);
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
    return s.substr(1, s.size() - 2);
  return s;
}

std::string keyword(const std::string& line)
{
  size_t i = 0;
  while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
    ++i;
  size_t j = i;
  while (j < line.size() && !std::isspace(static_cast<unsigned char>(line[j])))
    ++j;
  std::string k = line.substr(i, j - i);
  std::transform(k.begin(), k.end(), k.begin(),
                 [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return k;
}

std::string rest_after_keyword(const std::string& line)
{
  size_t i = 0;
  while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
    ++i;
  while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i])))
    ++i;
  return trim(line.substr(i));
}

gint64 index_to_ns(const std::string& mmssff)
{
  int m = 0;
  int s = 0;
  int f = 0;
  if (std::sscanf(mmssff.c_str(), "%d:%d:%d", &m, &s, &f) != 3)
    return -1;
  if (m < 0 || s < 0 || f < 0)
    return -1;
  const gint64 frames = static_cast<gint64>(m) * 60 * 75 + static_cast<gint64>(s) * 75 + f;
  return frames * (1000000000LL / 75);
}

std::string file_name_from_rest(const std::string& rest)
{
  std::string t = trim(rest);
  if (t.empty())
    return {};
  if (t.front() == '"') {
    const auto end = t.find('"', 1);
    if (end == std::string::npos)
      return {};
    return t.substr(1, end - 1);
  }
  const auto sp = t.find_first_of(" \t");
  if (sp == std::string::npos)
    return t;
  return t.substr(0, sp);
}

void flush_file(std::vector<CueFile>& out, CueFile& cur)
{
  if (cur.audio_path.empty() || cur.tracks.empty()) {
    cur = CueFile{};
    return;
  }
  out.push_back(cur);
  cur = CueFile{};
}

}  // namespace

std::vector<CueFile> parse_cue_sheet(const std::string& cue_path)
{
  std::ifstream in(cue_path);
  if (!in)
    return {};
  const std::string dir = Glib::path_get_dirname(cue_path);
  std::vector<CueFile> out;
  CueFile cur;
  CueTrack* track = nullptr;
  Glib::ustring sheet_performer;
  std::string line;
  if (in.peek() == '\xEF') {
    char bom[3] = {};
    in.read(bom, 3);
    if (!(bom[0] == '\xEF' && bom[1] == '\xBB' && bom[2] == '\xBF'))
      in.seekg(0);
  }
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    line = trim(line);
    if (line.empty())
      continue;
    const std::string key = keyword(line);
    const std::string rest = rest_after_keyword(line);
    if (key == "FILE") {
      flush_file(out, cur);
      track = nullptr;
      std::string name = file_name_from_rest(rest);
      std::replace(name.begin(), name.end(), '\\', '/');
      if (name.empty())
        continue;
      if (name[0] == '/' || (name.size() > 1 && name[1] == ':'))
        cur.audio_path = name;
      else
        cur.audio_path = Glib::build_filename(dir, name);
      cur.performer = sheet_performer;
    } else if (key == "PERFORMER") {
      const Glib::ustring p = unquote(rest);
      if (track)
        track->performer = p;
      else {
        sheet_performer = p;
        cur.performer = p;
      }
    } else if (key == "TITLE") {
      const Glib::ustring t = unquote(rest);
      if (track)
        track->title = t;
    } else if (key == "TRACK") {
      cur.tracks.push_back({});
      track = &cur.tracks.back();
      track->performer = cur.performer;
    } else if (key == "INDEX") {
      if (!track)
        continue;
      std::string idx_rest = rest;
      const auto sp = idx_rest.find_first_of(" \t");
      std::string num = idx_rest;
      std::string time;
      if (sp != std::string::npos) {
        num = trim(idx_rest.substr(0, sp));
        time = trim(idx_rest.substr(sp));
      }
      if (num == "01" || num == "1") {
        const gint64 ns = index_to_ns(time);
        if (ns >= 0)
          track->start_ns = ns;
      }
    }
  }
  flush_file(out, cur);
  for (auto& f : out) {
    f.tracks.erase(std::remove_if(f.tracks.begin(), f.tracks.end(),
                                  [](const CueTrack& t) { return t.start_ns < 0; }),
                   f.tracks.end());
  }
  out.erase(
      std::remove_if(out.begin(), out.end(), [](const CueFile& f) { return f.tracks.empty(); }),
      out.end());
  return out;
}

}  // namespace earblaster
