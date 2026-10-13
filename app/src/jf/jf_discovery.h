/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Finding Emby and Jellyfin servers on the local network using both protocol
 * UDP discovery questions on port 7359. A reachable server
 * answers with its name, id and address.
 */
#pragma once

#include <string>
#include <vector>

namespace jf {

struct FoundServer {
    std::string id, name, address;   /* address: "http://192.168.0.10:8096" */
};

/* Asks once and gathers the answers for timeout_ms. Blocks: call off the render thread. */
std::vector<FoundServer> discover(int timeout_ms = 1500);

} // namespace jf
