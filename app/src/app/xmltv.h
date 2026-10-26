/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later
 *
 * An XMLTV TV guide read as it downloads, plain or gzip-compressed, without
 * holding the file: big providers' guides run to hundreds of megabytes. */
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace xmltv {

struct Programme {
    std::string channel;            /* the <channel> id it belongs to */
    int64_t start = 0, end = 0;     /* Unix seconds */
    std::string title, description;
};

class Reader {
public:
    /* channel: each <channel> with its <display-name>s; programme: each
     * <programme> with a channel, a start and a title. */
    using ChannelFn = std::function<void(const std::string &id, const std::vector<std::string> &names)>;
    using ProgrammeFn = std::function<void(Programme &&)>;
    Reader(ChannelFn channel, ProgrammeFn programme);
    ~Reader();
    Reader(const Reader &) = delete;
    Reader &operator=(const Reader &) = delete;
    /* The next piece of the file. False: it isn't an XMLTV guide, or it is
     * broken beyond reading (error() says which). */
    bool feed(const char *data, size_t n);
    /* The end of the file. */
    bool finish();
    const std::string &error() const;
    size_t programmes() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

} // namespace xmltv
