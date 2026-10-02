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

/* What one top-level copy did. Counts are files; folders are merged. */
struct SyncCopyStats {
  int copied = 0;  /* files written */
  int existed = 0; /* files (or the top-level item) already at the destination */
  int failed = 0;  /* files that could not be copied */
  int folders_created = 0;
  bool cancelled = false;
  std::string error; /* first failure message */
};

/* Copy src (file or folder) into dest_dir under the same name. An existing
 * destination folder is merged: missing files are copied, existing files are
 * left alone and counted in `existed`. A file that fails is removed from the
 * destination and the rest of the folder is still copied, so a retry resumes. */
SyncCopyStats sync_copy_tree(const Glib::RefPtr<Gio::File>& src,
                             const Glib::RefPtr<Gio::File>& dest_dir,
                             const Glib::RefPtr<Gio::Cancellable>& cancellable,
                             const sigc::slot<void, Glib::ustring>& on_file);

}  // namespace earblaster
