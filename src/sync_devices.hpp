/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <glibmm/ustring.h>

#include <string>
#include <vector>

namespace earblaster {

/* Already-mounted USB mass-storage (TRAN=usb) and gio MTP volumes.
 * Host disks (nvme, /, /data) are never returned. Does not mount anything. */
struct SyncDevice {
  Glib::ustring label;
  std::string uri;
};

std::vector<SyncDevice> list_sync_devices();

}  // namespace earblaster
