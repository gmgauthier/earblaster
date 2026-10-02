/* SPDX-License-Identifier: Unlicense */

#include "open_payload.hpp"

#include <unistd.h>

#include <cerrno>

namespace earblaster {

std::string encode_open_payload(const std::vector<std::string>& paths)
{
  std::string out;
  for (const auto& p : paths) {
    if (p.empty())
      continue;
    out += p;
    out += '\n';
  }
  out += '\n';
  return out;
}

bool decode_open_payload(const std::string& payload, std::vector<std::string>& paths)
{
  paths.clear();
  if (payload.empty())
    return true;
  std::vector<std::string> out;
  size_t pos = 0;
  while (pos < payload.size()) {
    const size_t nl = payload.find('\n', pos);
    if (nl == std::string::npos)
      return false; /* last line cut off */
    if (nl == pos) {
      /* End marker. Anything after it is not ours. */
      if (nl + 1 != payload.size())
        return false;
      paths = std::move(out);
      return true;
    }
    out.push_back(payload.substr(pos, nl - pos));
    pos = nl + 1;
  }
  return false; /* no end marker */
}

bool write_all(int fd, const std::string& data)
{
  size_t done = 0;
  while (done < data.size()) {
    const ssize_t n = ::write(fd, data.data() + done, data.size() - done);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    if (n == 0)
      return false;
    done += static_cast<size_t>(n);
  }
  return true;
}

bool read_all(int fd, std::string& out, size_t max_bytes)
{
  out.clear();
  char tmp[65536];
  for (;;) {
    const ssize_t n = ::read(fd, tmp, sizeof(tmp));
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    if (n == 0)
      return true;
    out.append(tmp, static_cast<size_t>(n));
    if (out.size() > max_bytes)
      return false;
  }
}

}  // namespace earblaster
