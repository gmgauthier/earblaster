/* SPDX-License-Identifier: Unlicense */

#include "open_payload.hpp"
#include "check.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <string>
#include <thread>
#include <vector>

namespace {

std::vector<std::string> many_paths(size_t total_bytes)
{
  std::vector<std::string> out;
  size_t bytes = 0;
  for (int i = 0; bytes < total_bytes; ++i) {
    std::string p = "/music/library/artist " + std::to_string(i) + "/album/track " +
                    std::to_string(i) + " with a fairly long file name.flac";
    bytes += p.size() + 1;
    out.push_back(std::move(p));
  }
  return out;
}

void test_round_trip()
{
  const std::vector<std::string> paths = {"/a/one.flac", "/b/two words.mp3", "/c/x.cue"};
  std::vector<std::string> back;
  CHECK(earblaster::decode_open_payload(earblaster::encode_open_payload(paths), back));
  CHECK(back == paths);

  /* No paths: a plain "focus the window" request. */
  back = {"stale"};
  CHECK(earblaster::decode_open_payload(earblaster::encode_open_payload({}), back));
  CHECK(back.empty());
  CHECK(earblaster::decode_open_payload("", back));
  CHECK(back.empty());
}

void test_truncated_payload_is_rejected()
{
  /* Every cut-off prefix is refused, including one that ends on a newline. */
  const std::string full = earblaster::encode_open_payload({"/a/one.flac", "/b/two.flac"});
  for (size_t len = 1; len < full.size(); ++len) {
    std::vector<std::string> back;
    CHECK(!earblaster::decode_open_payload(full.substr(0, len), back));
  }
}

void test_large_payload_over_socket()
{
  /* More than 1 MB of paths arrives whole through a socket, despite short writes. */
  const auto paths = many_paths(3 * 1024 * 1024);
  int sv[2];
  CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
  bool sent = false;
  std::thread writer([&]() {
    sent = earblaster::write_all(sv[0], earblaster::encode_open_payload(paths));
    ::close(sv[0]);
  });
  std::string buf;
  const bool got = earblaster::read_all(sv[1], buf);
  writer.join();
  ::close(sv[1]);
  CHECK(sent);
  CHECK(got);
  std::vector<std::string> back;
  CHECK(earblaster::decode_open_payload(buf, back));
  CHECK(back.size() == paths.size());
  CHECK(back == paths);
}

}  // namespace

int main()
{
  test_round_trip();
  test_truncated_payload_is_rejected();
  test_large_payload_over_socket();
  return suite_test::done("open_payload");
}
