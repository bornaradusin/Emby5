/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "jf_discovery.h"
#include "server_kind.h"

#include "cJSON.h"

#include <arpa/inet.h>
#include <cstring>
#include <ctime>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace jf {
namespace {

double now_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

std::string str(const cJSON *o, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) && v->valuestring ? v->valuestring : "";
}

} // namespace

std::vector<FoundServer> discover(int timeout_ms)
{
    std::vector<FoundServer> out;
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return out;
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof on);
    struct sockaddr_in to;
    memset(&to, 0, sizeof to);
    to.sin_family = AF_INET;
    to.sin_port = htons(7359);
    to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    // Both protocols use port 7359, but their discovery request strings differ.
    const char *questions[] = {"who is EmbyServer?", "who is JellyfinServer?"};
    bool sent = false;
    for (const char *ask : questions)
        sent = (sendto(fd, ask, std::strlen(ask), 0, (const struct sockaddr *)&to, sizeof to) >= 0) || sent;
    if (!sent) { close(fd); return out; }
    const double end = now_ms() + timeout_ms;
    for (;;) {
        const int left = (int)(end - now_ms());
        if (left <= 0)
            break;
        struct pollfd p = {fd, POLLIN, 0};
        if (poll(&p, 1, left) <= 0)
            break;
        char buf[2048];
        const ssize_t n = recvfrom(fd, buf, sizeof buf - 1, 0, nullptr, nullptr);
        if (n <= 0)
            continue;
        buf[n] = 0;
        cJSON *j = cJSON_Parse(buf);
        if (!j)
            continue;
        FoundServer s{str(j, "Id"), str(j, "Name"), str(j, "Address")};
        cJSON_Delete(j);
        s.address = server_url::normalize(s.address);
        if (s.address.empty())
            continue;
        bool seen = false;
        for (const FoundServer &o : out)
            seen = seen || o.address == s.address;
        if (!seen)
            out.push_back(s);
    }
    close(fd);
    return out;
}

} // namespace jf
