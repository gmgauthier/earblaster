/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <string>
#include <vector>

namespace earblaster {

/* Second-instance hand-off: one path per line, then an empty line that marks
 * the list as complete. Paths are never empty, so the marker is unambiguous. */
std::string encode_open_payload(const std::vector<std::string>& paths);

/* False if the payload is not complete (no end marker): the sender was cut
 * off, and the primary must not replace its playlist with a partial list.
 * An empty payload is a bare "focus the window" request. */
bool decode_open_payload(const std::string& payload, std::vector<std::string>& paths);

/* Write all of data, retrying short writes and EINTR. */
bool write_all(int fd, const std::string& data);

/* Read to EOF. False on a read error or past max_bytes. */
bool read_all(int fd, std::string& out, size_t max_bytes = 64u * 1024u * 1024u);

}  // namespace earblaster
