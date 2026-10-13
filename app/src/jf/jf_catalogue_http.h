/* Streaming catalogue transport, separate from playback and user-data writes.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace jf {
struct CatalogueHttpResult {
    bool complete=false, cancelled=false, transport_error=false;
    int status=0;
    uint64_t bytes=0;
    size_t items=0;
    std::string error;
};
// item receives one complete JSON object. Returning false stops the transfer.
// activity is called after bytes arrive, never while merely waiting on a socket.
// A long but progressing transfer is not cut off by a small absolute API timeout.
CatalogueHttpResult http_catalogue_array(const std::string &url,
    const std::function<bool(const std::string &)> &item,
    const std::function<bool()> &cancelled={},
    const std::function<void(uint64_t)> &activity={},
    int idle_seconds=45);
}
