/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cctype>
#include <string>
namespace ui {
inline std::string search_text(std::string q) {
    const auto first = q.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    q = q.substr(first, q.find_last_not_of(" \t\r\n") - first + 1);
    if (q.size() > 200) {
        size_t end = 200;
        while (end && ((unsigned char)q[end] & 0xc0) == 0x80) --end;
        q.resize(end);
    }
    return q;
}
inline std::string search_fold(std::string s) {
    for (auto &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
} // namespace ui
