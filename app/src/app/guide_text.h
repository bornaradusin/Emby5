/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Small helpers the TV guide's sources share: times in the forms the servers
 * and XMLTV files give them, and Xtream's base64 titles. */
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

namespace guide_text {

/* Days since 1970-01-01 of a civil date (proleptic Gregorian). */
inline int64_t days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (unsigned)(m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

/* ISO-8601 with Z or a numeric offset ("2026-10-26T00:30:00.0000000Z"), as
 * Unix seconds; 0 when it isn't a time. */
inline int64_t iso_epoch(const std::string &s)
{
    int y = 0, m = 0, d = 0, h = 0, mi = 0, sec = 0;
    if (std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d", &y, &m, &d, &h, &mi, &sec) < 5) return 0;
    if (m < 1 || m > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || sec > 60) return 0;
    int64_t epoch = days_from_civil(y, m, d) * 86400 + h * 3600 + mi * 60 + sec;
    if (s.size() > 19) {
        const auto tz = s.find_first_of("+-", 19);
        if (tz != std::string::npos) {
            int oh = 0, om = 0;
            if (std::sscanf(s.c_str() + tz + 1, "%2d:%2d", &oh, &om) >= 1)
                epoch += (s[tz] == '+' ? -1 : 1) * (oh * 3600 + om * 60);
        }
    }
    return epoch;
}

/* XMLTV's "20261026003000 +0100" (the offset optional, then UTC), as Unix
 * seconds; 0 when it isn't a time. */
inline int64_t xmltv_epoch(const std::string &s)
{
    int y = 0, m = 0, d = 0, h = 0, mi = 0, sec = 0;
    if (s.size() < 12) return 0;
    for (size_t i = 0; i < 12; i++)
        if (s[i] < '0' || s[i] > '9') return 0;
    if (std::sscanf(s.c_str(), "%4d%2d%2d%2d%2d", &y, &m, &d, &h, &mi) != 5) return 0;
    if (s.size() >= 14 && s[12] >= '0' && s[12] <= '9') sec = (s[12] - '0') * 10 + (s[13] - '0');
    if (m < 1 || m > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || sec > 60) return 0;
    int64_t epoch = days_from_civil(y, m, d) * 86400 + h * 3600 + mi * 60 + sec;
    const auto tz = s.find_first_of("+-", 12);
    if (tz != std::string::npos && tz + 5 <= s.size()) {
        int oh = 0, om = 0;
        if (std::sscanf(s.c_str() + tz + 1, "%2d%2d", &oh, &om) == 2)
            epoch += (s[tz] == '+' ? -1 : 1) * (oh * 3600 + om * 60);
    }
    return epoch;
}

/* Xtream sends titles and descriptions base64-encoded; anything that isn't
 * (or doesn't decode to text) stays as it is. */
inline std::string base64_text(const std::string &s)
{
    static const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (s.size() < 4 || s.size() % 4) return s;
    std::string out;
    unsigned value = 0;
    int bits = -8;
    for (unsigned char c : s) {
        if (c == '=') break;
        const size_t pos = alphabet.find((char)c);
        if (pos == std::string::npos) return s;
        value = (value << 6) | (unsigned)pos;
        bits += 6;
        if (bits >= 0) {
            out.push_back((char)((value >> bits) & 255));
            bits -= 8;
        }
    }
    for (unsigned char c : out)
        if (c < 32 && c != '\t' && c != '\n' && c != '\r') return s;
    return out.empty() ? s : out;
}

/* At most max bytes, cut at a character boundary, with "…" when cut. */
inline std::string clip(const std::string &s, size_t max)
{
    if (s.size() <= max) return s;
    size_t n = max;
    while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
    return s.substr(0, n) + "\xE2\x80\xA6";
}

} // namespace guide_text
