/* SPDX-License-Identifier: Unlicense */

#include "cover_art.hpp"

#include <gdkmm/pixbufloader.h>
#include <glib.h>

#include <taglib/fileref.h>
#include <taglib/tbytevector.h>
#include <taglib/tpropertymap.h>
#include <taglib/tvariant.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace earblaster {
namespace {

namespace fs = std::filesystem;

std::string to_lower(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

std::string uri_to_path(const std::string& path_or_uri)
{
  if (path_or_uri.find("://") == std::string::npos)
    return path_or_uri;
  GError* err = nullptr;
  gchar* path = g_filename_from_uri(path_or_uri.c_str(), nullptr, &err);
  if (!path) {
    if (err)
      g_error_free(err);
    return {};
  }
  std::string out(path);
  g_free(path);
  return out;
}

Glib::RefPtr<Gdk::Pixbuf> pixbuf_from_bytes(const char* data, unsigned int size)
{
  if (!data || size == 0)
    return {};
  try {
    auto loader = Gdk::PixbufLoader::create();
    loader->write(reinterpret_cast<const guint8*>(data), size);
    loader->close();
    if (auto pix = loader->get_pixbuf())
      return pix->copy();
  } catch (const Glib::Error&) {
  }
  return {};
}

Glib::RefPtr<Gdk::Pixbuf> pixbuf_from_file(const fs::path& path)
{
  try {
    return Gdk::Pixbuf::create_from_file(path.string());
  } catch (const Glib::Error&) {
    return {};
  }
}

Glib::RefPtr<Gdk::Pixbuf> from_taglib(const std::string& path)
{
  TagLib::FileRef ref(path.c_str(), true);
  if (ref.isNull())
    return {};
  const auto pics = ref.complexProperties("PICTURE");
  TagLib::ByteVector fallback;
  for (const auto& map : pics) {
    const auto data_it = map.find("data");
    if (data_it == map.end())
      continue;
    bool ok = false;
    const TagLib::ByteVector data = data_it->second.toByteVector(&ok);
    if (!ok || data.isEmpty())
      continue;
    TagLib::String type;
    const auto type_it = map.find("pictureType");
    if (type_it != map.end())
      type = type_it->second.toString();
    if (type == "Front Cover")
      return pixbuf_from_bytes(data.data(), data.size());
    if (fallback.isEmpty())
      fallback = data;
  }
  if (!fallback.isEmpty())
    return pixbuf_from_bytes(fallback.data(), fallback.size());
  return {};
}

bool is_sidecar_name(const std::string& lower)
{
  static const char* names[] = {
      "folder.jpg",    "folder.jpeg", "folder.png", "folder.webp",
      "cover.jpg",     "cover.jpeg",  "cover.png",  "cover.webp",
      "albumart.jpg",  "albumart.jpeg", "albumart.png",
      "front.jpg",     "front.jpeg",  "front.png",
      nullptr};
  for (int i = 0; names[i]; ++i) {
    if (lower == names[i])
      return true;
  }
  return false;
}

Glib::RefPtr<Gdk::Pixbuf> from_sidecar(const std::string& track_path)
{
  std::error_code ec;
  const fs::path dir = fs::path(track_path).parent_path();
  if (dir.empty() || !fs::is_directory(dir, ec))
    return {};
  std::vector<fs::path> matches;
  for (const auto& entry : fs::directory_iterator(dir, ec)) {
    if (ec || !entry.is_regular_file(ec))
      continue;
    if (is_sidecar_name(to_lower(entry.path().filename().string())))
      matches.push_back(entry.path());
  }
  std::sort(matches.begin(), matches.end());
  for (const auto& p : matches) {
    if (auto pix = pixbuf_from_file(p))
      return pix;
  }
  return {};
}

}  // namespace

Glib::RefPtr<Gdk::Pixbuf> load_cover(const std::string& path_or_uri)
{
  const std::string path = uri_to_path(path_or_uri);
  if (path.empty())
    return {};
  if (auto pix = from_taglib(path))
    return pix;
  return from_sidecar(path);
}

}  // namespace earblaster
