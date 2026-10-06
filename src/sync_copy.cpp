/* SPDX-License-Identifier: Unlicense */

#include "sync_copy.hpp"

#include <algorithm>
#include <vector>

namespace earblaster {
namespace {

const char* kListAttrs =
    "standard::name,standard::type,standard::size,standard::display-name,standard::is-hidden,"
    "id::file";

/* Folder identity (device + inode on local disks). Empty where the backend
 * has none, e.g. MTP, which has no symlinks either. */
std::string folder_id(const Glib::RefPtr<Gio::FileInfo>& info)
{
  if (!info || !info->has_attribute("id::file"))
    return {};
  return info->get_attribute_string("id::file");
}

void copy_tree_in(const Glib::RefPtr<Gio::File>& src, const Glib::RefPtr<Gio::File>& dest_dir,
                  const Glib::RefPtr<Gio::Cancellable>& cancellable,
                  const sigc::slot<void, Glib::ustring>& on_file,
                  std::vector<std::string>& ancestors, SyncCopyStats& st);

bool is_cancelled(const Glib::RefPtr<Gio::Cancellable>& cancellable)
{
  return cancellable && cancellable->is_cancelled();
}

void note_failure(SyncCopyStats& st, const Glib::ustring& what)
{
  ++st.failed;
  if (st.error.empty())
    st.error = what;
}

}  // namespace

bool sync_name_skipped(const std::string& name)
{
  return name.empty() || name[0] == '.' || name == ".." || name == ".";
}

bool sync_info_hidden(const Glib::RefPtr<Gio::FileInfo>& info)
{
  if (!info || !info->has_attribute("standard::is-hidden"))
    return false;
  return info->is_hidden();
}

bool sync_dir_type(Gio::FileType type)
{
  return type == Gio::FILE_TYPE_DIRECTORY || type == Gio::FILE_TYPE_MOUNTABLE;
}

SyncCopyStats sync_copy_tree(const Glib::RefPtr<Gio::File>& src,
                             const Glib::RefPtr<Gio::File>& dest_dir,
                             const Glib::RefPtr<Gio::Cancellable>& cancellable,
                             const sigc::slot<void, Glib::ustring>& on_file)
{
  SyncCopyStats st;
  std::vector<std::string> ancestors;
  try {
    copy_tree_in(src, dest_dir, cancellable, on_file, ancestors, st);
  } catch (const Glib::Error& e) {
    if (e.code() != Gio::Error::CANCELLED)
      throw;
    st.cancelled = true;
  }
  /* Cancel can land between two files, where nothing throws. */
  if (is_cancelled(cancellable))
    st.cancelled = true;
  return st;
}

namespace {

void copy_tree_in(const Glib::RefPtr<Gio::File>& src, const Glib::RefPtr<Gio::File>& dest_dir,
                  const Glib::RefPtr<Gio::Cancellable>& cancellable,
                  const sigc::slot<void, Glib::ustring>& on_file,
                  std::vector<std::string>& ancestors, SyncCopyStats& st)
{
  if (!src || !dest_dir)
    return;
  if (is_cancelled(cancellable))
    return;
  const std::string name = src->get_basename();
  if (sync_name_skipped(name))
    return;
  Glib::RefPtr<Gio::FileInfo> info;
  try {
    info = src->query_info(cancellable, kListAttrs);
  } catch (const Glib::Error& e) {
    if (e.code() == Gio::Error::CANCELLED)
      throw;
    note_failure(st, e.what());
    return;
  }
  if (!info)
    return;
  const auto type = info->get_file_type();
  auto dest = dest_dir->get_child(name);

  if (sync_dir_type(type)) {
    /* Enumeration follows symlinks, so a link to this folder or an ancestor
     * reports as a folder. Do not walk into a folder already being copied. */
    const std::string id = folder_id(info);
    if (!id.empty() && std::find(ancestors.begin(), ancestors.end(), id) != ancestors.end())
      return;
    /* An existing folder is merged so an interrupted copy can be resumed.
     * Do not follow a symlink: merging through it writes outside this tree.
     * A missing name is UNKNOWN and is created; a link, including a dangling
     * one, is an existing item. */
    const auto dest_type =
        dest->query_file_type(Gio::FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancellable);
    if (dest_type != Gio::FILE_TYPE_UNKNOWN && !sync_dir_type(dest_type)) {
      ++st.existed;
      return;
    }
    if (dest_type == Gio::FILE_TYPE_UNKNOWN) {
      try {
        dest->make_directory(cancellable);
        ++st.folders_created;
      } catch (const Glib::Error& e) {
        if (e.code() == Gio::Error::CANCELLED)
          throw;
        note_failure(st, e.what());
        return;
      }
    }
    Glib::RefPtr<Gio::FileEnumerator> en;
    try {
      en = src->enumerate_children(cancellable, kListAttrs);
    } catch (const Glib::Error& e) {
      if (e.code() == Gio::Error::CANCELLED)
        throw;
      note_failure(st, e.what());
      return;
    }
    if (!en)
      return;
    ancestors.push_back(id);
    struct Pop {
      std::vector<std::string>& v;
      ~Pop()
      {
        v.pop_back();
      }
    } pop{ancestors};
    while (auto child = en->next_file(cancellable)) {
      if (is_cancelled(cancellable))
        return;
      const std::string child_name = child->get_name();
      if (sync_name_skipped(child_name) || sync_info_hidden(child))
        continue;
      const auto ct = child->get_file_type();
      if (sync_dir_type(ct) || ct == Gio::FILE_TYPE_REGULAR)
        copy_tree_in(src->get_child(child_name), dest, cancellable, on_file, ancestors, st);
    }
    return;
  }
  if (type != Gio::FILE_TYPE_REGULAR)
    return;
  if (dest->query_exists(cancellable)) {
    ++st.existed;
    return;
  }
  on_file(Glib::ustring(info->get_display_name().empty() ? name : info->get_display_name()));
  try {
    src->copy(dest, [](goffset, goffset) {}, cancellable, Gio::FILE_COPY_NONE);
    ++st.copied;
  } catch (const Glib::Error& e) {
    /* Never leave a partial file behind: a retry would take it as done. */
    if (e.code() != Gio::Error::EXISTS) {
      try {
        dest->remove();
      } catch (const Glib::Error&) {
      }
    }
    if (e.code() == Gio::Error::CANCELLED)
      throw;
    if (e.code() == Gio::Error::EXISTS) {
      ++st.existed;
      return;
    }
    note_failure(st, e.what());
  }
}

}  // namespace

}  // namespace earblaster
