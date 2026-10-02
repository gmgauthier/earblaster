/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <giomm.h>

#include <string>

namespace earblaster {

/* Names the Sync window never lists or copies (empty, dot-files). */
bool sync_name_skipped(const std::string& name);

/* MTP (and some FUSE backends) omit attributes even when requested.
 * is_hidden() CRITICAL-aborts if standard::is-hidden is missing. */
bool sync_info_hidden(const Glib::RefPtr<Gio::FileInfo>& info);

bool sync_dir_type(Gio::FileType type);

/* Copy src (file or folder) into dest_dir under the same name.
 * Returns false if dest already has this name (file or folder). Does not merge. */
bool sync_copy_tree(const Glib::RefPtr<Gio::File>& src, const Glib::RefPtr<Gio::File>& dest_dir,
                    const Glib::RefPtr<Gio::Cancellable>& cancellable,
                    const sigc::slot<void, Glib::ustring>& on_file);

}  // namespace earblaster
