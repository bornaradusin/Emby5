/* Emby5 independent updater: streamed HTTPS downloads and digest verification. */
#pragma once
#include <string>
#include <cstdint>
namespace update_download {
struct Result { bool ok=false; std::string error; std::string sha256; uint64_t bytes=0; };
Result fetch_verified(const std::string &url, const std::string &expected_sha256, const std::string &dest);
}
