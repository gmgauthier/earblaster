/* SPDX-License-Identifier: Unlicense */

#include "sync_copy.hpp"
#include "check.hpp"

#include <giomm.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;

void write_file(const fs::path& p, const std::string& body = "data")
{
  std::ofstream out(p, std::ios::binary);
  out << body;
}

struct TempDir {
  fs::path root;
  explicit TempDir(const std::string& tag)
  {
    root = fs::temp_directory_path() /
           ("earblaster-sync-" + tag + "-" + std::to_string(static_cast<long long>(getpid())));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);
  }
  ~TempDir()
  {
    std::error_code ec;
    fs::remove_all(root, ec);
  }
};

Glib::RefPtr<Gio::File> gfile(const fs::path& p)
{
  return Gio::File::create_for_path(p.string());
}

int count_entries(const fs::path& dir)
{
  int n = 0;
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(dir, ec); it != fs::recursive_directory_iterator();
       it.increment(ec)) {
    if (ec)
      break;
    ++n;
  }
  return n;
}

void test_symlink_loop_terminates()
{
  /* album/loop -> album and album/sub/up -> .. : the copy finishes and each
   * real file lands once. */
  TempDir t("loop");
  const fs::path album = t.root / "src" / "album";
  fs::create_directories(album / "sub");
  write_file(album / "01.flac");
  write_file(album / "sub" / "02.flac");
  fs::create_directory_symlink(album, album / "loop");
  fs::create_directory_symlink("..", album / "sub" / "up");
  const fs::path dest = t.root / "dest";
  fs::create_directories(dest);

  auto cancel = Gio::Cancellable::create();
  int files = 0;
  bool ok = false;
  try {
    ok = earblaster::sync_copy_tree(gfile(album), gfile(dest), cancel,
                                    [&](const Glib::ustring&) { ++files; });
  } catch (const Glib::Error& e) {
    std::cerr << "copy threw: " << e.what() << "\n";
  }
  CHECK(ok);
  CHECK(fs::is_regular_file(dest / "album" / "01.flac"));
  CHECK(fs::is_regular_file(dest / "album" / "sub" / "02.flac"));
  CHECK(!fs::exists(dest / "album" / "loop" / "loop"));
  CHECK(!fs::exists(dest / "album" / "sub" / "up" / "sub"));
  CHECK(files == 2);
  CHECK(count_entries(dest) == 4);
}

void test_symlink_to_outside_folder_is_copied()
{
  /* A link to a folder outside the tree (not an ancestor) is still copied. */
  TempDir t("outside");
  const fs::path other = t.root / "elsewhere" / "bonus";
  fs::create_directories(other);
  write_file(other / "b.flac");
  const fs::path album = t.root / "src" / "album";
  fs::create_directories(album);
  write_file(album / "a.flac");
  fs::create_directory_symlink(other, album / "bonus");
  const fs::path dest = t.root / "dest";
  fs::create_directories(dest);

  auto cancel = Gio::Cancellable::create();
  CHECK(earblaster::sync_copy_tree(gfile(album), gfile(dest), cancel, [](const Glib::ustring&) {}));
  CHECK(fs::is_regular_file(dest / "album" / "a.flac"));
  CHECK(fs::is_regular_file(dest / "album" / "bonus" / "b.flac"));
}

}  // namespace

int main()
{
  Gio::init();
  test_symlink_loop_terminates();
  test_symlink_to_outside_folder_is_copied();
  return suite_test::done("sync_copy");
}
