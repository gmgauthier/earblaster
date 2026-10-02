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

bool copy_tree_in(const Glib::RefPtr<Gio::File>& src, const Glib::RefPtr<Gio::File>& dest_dir,
                  const Glib::RefPtr<Gio::Cancellable>& cancellable,
                  const sigc::slot<void, Glib::ustring>& on_file,
                  std::vector<std::string>& ancestors);

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

bool sync_copy_tree(const Glib::RefPtr<Gio::File>& src, const Glib::RefPtr<Gio::File>& dest_dir,
                    const Glib::RefPtr<Gio::Cancellable>& cancellable,
                    const sigc::slot<void, Glib::ustring>& on_file)
{
  std::vector<std::string> ancestors;
  return copy_tree_in(src, dest_dir, cancellable, on_file, ancestors);
}

namespace {

bool copy_tree_in(const Glib::RefPtr<Gio::File>& src, const Glib::RefPtr<Gio::File>& dest_dir,
                  const Glib::RefPtr<Gio::Cancellable>& cancellable,
                  const sigc::slot<void, Glib::ustring>& on_file,
                  std::vector<std::string>& ancestors)
{
  if (!src || !dest_dir)
    return false;
  if (cancellable && cancellable->is_cancelled())
    return false;
  auto info = src->query_info(cancellable, kListAttrs);
  if (!info)
    return false;
  const auto type = info->get_file_type();
  const std::string name = src->get_basename();
  if (sync_name_skipped(name))
    return false;
  auto dest = dest_dir->get_child(name);
  if (dest->query_exists(cancellable))
    return false;
  if (sync_dir_type(type)) {
    /* Enumeration follows symlinks, so a link to this folder or an ancestor
     * reports as a folder. Do not walk into a folder already being copied. */
    const std::string id = folder_id(info);
    if (!id.empty() && std::find(ancestors.begin(), ancestors.end(), id) != ancestors.end())
      return false;
    dest->make_directory(cancellable);
    auto en = src->enumerate_children(cancellable, kListAttrs);
    if (!en)
      return true;
    ancestors.push_back(id);
    struct Pop {
      std::vector<std::string>& v;
      ~Pop()
      {
        v.pop_back();
      }
    } pop{ancestors};
    while (auto child = en->next_file(cancellable)) {
      if (cancellable && cancellable->is_cancelled())
        return true;
      const std::string child_name = child->get_name();
      if (sync_name_skipped(child_name) || sync_info_hidden(child))
        continue;
      const auto ct = child->get_file_type();
      if (sync_dir_type(ct) || ct == Gio::FILE_TYPE_REGULAR)
        copy_tree_in(src->get_child(child_name), dest, cancellable, on_file, ancestors);
    }
    return true;
  }
  if (type != Gio::FILE_TYPE_REGULAR)
    return false;
  on_file(Glib::ustring(info->get_display_name().empty() ? name : info->get_display_name()));
  src->copy(dest, [](goffset, goffset) {}, cancellable, Gio::FILE_COPY_NONE);
  return true;
}

}  // namespace

}  // namespace earblaster
