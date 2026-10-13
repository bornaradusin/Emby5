/* Emby5 dual-server routing. SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace jf {
enum class ServerKind { Emby, Jellyfin };
inline const char *server_kind_id(ServerKind kind)
{
    return kind == ServerKind::Jellyfin ? "jellyfin" : "emby";
}
inline const char *server_kind_name(ServerKind kind)
{
    return kind == ServerKind::Jellyfin ? "Jellyfin" : "Emby";
}
inline ServerKind server_kind_from_id(const std::string &id)
{
    return id == "jellyfin" ? ServerKind::Jellyfin : ServerKind::Emby;
}

namespace server_url {
inline std::string lower(std::string s)
{
    for (char &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
inline bool starts(const std::string &s, const std::string &p) { return s.compare(0, p.size(), p) == 0; }
inline bool ends(const std::string &s, const std::string &p)
{
    return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}
inline bool http(const std::string &s) { return starts(s, "http://") || starts(s, "https://"); }
inline size_t path_start(const std::string &s)
{
    const size_t a = s.find("://");
    return a == std::string::npos ? std::string::npos : s.find('/', a + 3);
}
inline std::string origin(const std::string &s) { return s.substr(0, path_start(s)); }
inline std::string base_path(const std::string &s)
{
    const size_t p = path_start(s);
    return p == std::string::npos ? "" : s.substr(p);
}
inline bool under(const std::string &path, const std::string &base)
{
    return base.empty() || path == base || starts(path, base + "/") || starts(path, base + "?");
}
inline std::string normalize(std::string s)
{
    while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
    if (s.empty()) return s;
    // Reject URL credentials, query secrets and non-HTTP protocols at the address field.
    if (s.find_first_of("?#\\\r\n\t ") != std::string::npos) return "";
    const bool bare = s.find("://") == std::string::npos;
    if (bare) s = "http://" + s;
    const size_t scheme = s.find("://");
    s.replace(0, scheme, lower(s.substr(0, scheme)));
    if (!http(s)) return "";
    const size_t begin = s.find("://") + 3, slash = s.find('/', begin);
    const std::string host = s.substr(begin, slash - begin);
    if (host.empty() || host.find('@') != std::string::npos) return "";
    // Default 8096 for a bare hostname; preserve explicit schemes and proxy paths.
    bool port = host.find(':') != std::string::npos;
    if (host.front() == '[') {
        const size_t end = host.find(']');
        if (end == std::string::npos) return "";
        port = end + 1 < host.size();
    }
    if (bare && !port && slash == std::string::npos) s += ":8096";
    while (!s.empty() && s.back() == '/') s.pop_back();
    const std::string path = base_path(s);
    const std::string lo = lower(path);
    if (path.find("/../") != std::string::npos || ends(path, "/..") ||
        path.find("/./") != std::string::npos || ends(path, "/.") ||
        lo.find("%2e") != std::string::npos || lo.find("%2f") != std::string::npos ||
        lo.find("%5c") != std::string::npos) return "";
    return s;
}

// API routes are relative to the configured server, including a reverse-proxy base path.
// Absolute media URLs are allowed only inside that same origin/base: session headers must
// never be forwarded to an arbitrary host returned in a media-source response.
inline std::string join(const std::string &server, ServerKind kind, std::string path)
{
    if (!http(server) || path.find_first_of("\r\n\\") != std::string::npos) return "";
    // Reject path traversal before attaching session credentials. Query values can
    // contain arbitrary encoded tokens; inspect only the path, not its query.
    const std::string resource = http(path) ? base_path(path) : path;
    const std::string rawpath = resource.substr(0, resource.find_first_of("?#"));
    const std::string lo = lower(rawpath);
    if (starts(rawpath, "../") || rawpath == ".." || rawpath == "." || starts(rawpath, "./") ||
        rawpath.find("/../") != std::string::npos || ends(rawpath, "/..") ||
        rawpath.find("/./") != std::string::npos || ends(rawpath, "/.") ||
        lo.find("%2e") != std::string::npos || lo.find("%2f") != std::string::npos ||
        lo.find("%5c") != std::string::npos || path.find('#') != std::string::npos) return "";
    const std::string base = base_path(server);
    if (http(path)) {
        if (lower(origin(path)) != lower(origin(server)) || !under(base_path(path), base)) return "";
        return path;
    }
    if (path.find("://") != std::string::npos || starts(path, "//")) return "";
    if (path.empty() || path.front() != '/') path.insert(path.begin(), '/');
    // A returned absolute path may already contain the proxy base prefix.
    if (!base.empty() && under(path, base)) return origin(server) + path;
    if (kind == ServerKind::Emby) {
        const std::string root = ends(base, "/emby") ? server : server + "/emby";
        if (under(path, "/emby")) {
            // /emby is the protocol prefix, not another copy of the proxy base.
            return (ends(base, "/emby") ? server.substr(0, server.size() - 5) : server) + path;
        }
        return root + path;
    }
    return server + path;
}

// Canonical Jellyfin routes (10.9+). Emby's older, user-scoped routes remain untouched.
// Keeping this translation central also covers the existing playback and UI modules.
inline std::string jellyfin_route(const std::string &input)
{
    if (!starts(input, "/Users/")) return input;
    const size_t q = input.find('?');
    const std::string path = input.substr(0, q);
    const size_t user_end = path.find('/', 7);
    if (user_end == std::string::npos) return input; // /Users/{id}, /Users/Me, auth
    const std::string user = path.substr(7, user_end - 7);
    const std::string rest = path.substr(user_end);
    std::string target;
    if (rest == "/Views") target = "/UserViews";
    else if (rest == "/Items/Resume") target = "/UserItems/Resume";
    else if (rest == "/Items/Latest") target = "/Items/Latest";
    else if (rest == "/Configuration") target = "/Users/Configuration";
    else if (starts(rest, "/PlayedItems/")) target = "/UserPlayedItems/" + rest.substr(13);
    else if (starts(rest, "/FavoriteItems/")) target = "/UserFavoriteItems/" + rest.substr(15);
    else if (starts(rest, "/Items/") && ends(rest, "/UserData")) target = "/UserItems/" + rest.substr(7);
    else if (starts(rest, "/Items/")) target = rest;
    else return input;
    return target + "?userId=" + user + (q == std::string::npos ? "" : "&" + input.substr(q + 1));
}
} // namespace server_url
} // namespace jf
