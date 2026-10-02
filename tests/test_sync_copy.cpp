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
  earblaster::SyncCopyStats st;
  try {
    st = earblaster::sync_copy_tree(gfile(album), gfile(dest), cancel,
                                    [&](const Glib::ustring&) { ++files; });
  } catch (const Glib::Error& e) {
    std::cerr << "copy threw: " << e.what() << "\n";
  }
  CHECK(st.copied == 2);
  CHECK(st.failed == 0);
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
  CHECK(earblaster::sync_copy_tree(gfile(album), gfile(dest), cancel, [](const Glib::ustring&) {})
            .copied == 2);
  CHECK(fs::is_regular_file(dest / "album" / "a.flac"));
  CHECK(fs::is_regular_file(dest / "album" / "bonus" / "b.flac"));
}

void test_failed_folder_copy_can_be_resumed()
{
  /* One file fails mid-folder: the rest of the folder is still copied, no
   * partial file is left, and a retry copies only what is missing. */
  TempDir t("resume");
  const fs::path album = t.root / "src" / "album";
  fs::create_directories(album);
  write_file(album / "01.flac");
  write_file(album / "02.flac");
  write_file(album / "03.flac");
  const fs::path dest = t.root / "dest";
  fs::create_directories(dest);
  fs::permissions(album / "02.flac", fs::perms::none);
  const bool can_fail = !std::ifstream(album / "02.flac").good(); /* false when run as root */

  auto cancel = Gio::Cancellable::create();
  earblaster::SyncCopyStats first;
  try {
    first = earblaster::sync_copy_tree(gfile(album), gfile(dest), cancel,
                                       [](const Glib::ustring&) {});
  } catch (const Glib::Error& e) {
    std::cerr << "copy threw: " << e.what() << "\n";
    CHECK(false);
  }
  CHECK(fs::is_regular_file(dest / "album" / "01.flac"));
  CHECK(fs::is_regular_file(dest / "album" / "03.flac"));
  if (can_fail) {
    CHECK(first.failed == 1);
    CHECK(first.copied == 2);
    CHECK(!first.error.empty());
    CHECK(!fs::exists(dest / "album" / "02.flac"));
  }

  fs::permissions(album / "02.flac", fs::perms::owner_read | fs::perms::owner_write);
  const auto retry =
      earblaster::sync_copy_tree(gfile(album), gfile(dest), cancel, [](const Glib::ustring&) {});
  CHECK(fs::is_regular_file(dest / "album" / "02.flac"));
  CHECK(retry.failed == 0);
  CHECK(retry.copied == (can_fail ? 1 : 0));
  CHECK(retry.existed == (can_fail ? 2 : 3));
  CHECK(!retry.cancelled);
}

void test_existing_item_is_reported_as_existing()
{
  TempDir t("exists");
  const fs::path src = t.root / "src";
  fs::create_directories(src / "album");
  write_file(src / "album" / "a.flac");
  write_file(src / "single.flac");
  const fs::path dest = t.root / "dest";
  fs::create_directories(dest / "album");
  write_file(dest / "album" / "a.flac", "old");
  write_file(dest / "single.flac", "old");
  auto cancel = Gio::Cancellable::create();

  const auto folder =
      earblaster::sync_copy_tree(gfile(src / "album"), gfile(dest), cancel, [](const Glib::ustring&) {});
  CHECK(folder.copied == 0);
  CHECK(folder.existed == 1);
  CHECK(folder.failed == 0);
  const auto single =
      earblaster::sync_copy_tree(gfile(src / "single.flac"), gfile(dest), cancel, [](const Glib::ustring&) {});
  CHECK(single.copied == 0);
  CHECK(single.existed == 1);
  /* Existing files are never overwritten. */
  std::ifstream in(dest / "single.flac");
  std::string body;
  in >> body;
  CHECK(body == "old");
}

void test_cancel_during_folder_is_reported()
{
  /* Cancel while a folder's children are copying: the result says cancelled,
   * not copied, and the remaining files are not written. */
  TempDir t("cancel");
  const fs::path album = t.root / "src" / "album";
  fs::create_directories(album / "disc2");
  for (const char* n : {"01.flac", "02.flac", "03.flac"})
    write_file(album / n);
  write_file(album / "disc2" / "04.flac");
  const fs::path dest = t.root / "dest";
  fs::create_directories(dest);

  auto cancel = Gio::Cancellable::create();
  int started = 0;
  earblaster::SyncCopyStats st;
  bool threw = false;
  try {
    st = earblaster::sync_copy_tree(gfile(album), gfile(dest), cancel, [&](const Glib::ustring&) {
      if (++started == 1)
        cancel->cancel();
    });
  } catch (const Glib::Error&) {
    threw = true;
  }
  CHECK(!threw);
  CHECK(st.cancelled);
  CHECK(started == 1);
  CHECK(st.copied == 0);
  /* The file whose copy was cut off is not left half-written. (Empty
   * folders may remain; a later Transfer merges into them.) */
  int files_left = 0;
  for (const auto& e : fs::recursive_directory_iterator(dest))
    if (e.is_regular_file())
      ++files_left;
  CHECK(files_left == 0);

  /* Already cancelled before the folder starts. */
  auto pre = Gio::Cancellable::create();
  pre->cancel();
  const auto st2 = earblaster::sync_copy_tree(gfile(album), gfile(t.root / "dest"), pre,
                                              [](const Glib::ustring&) {});
  CHECK(st2.cancelled);
  CHECK(st2.copied == 0);
}

}  // namespace

int main()
{
  Gio::init();
  test_symlink_loop_terminates();
  test_symlink_to_outside_folder_is_copied();
  test_failed_folder_copy_can_be_resumed();
  test_existing_item_is_reported_as_existing();
  test_cancel_during_folder_is_reported();
  return suite_test::done("sync_copy");
}
