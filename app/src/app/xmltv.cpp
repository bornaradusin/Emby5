/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/xmltv.h"
#include "app/guide_text.h"
#include <algorithm>
#include <cstring>
#include <mutex>
#include <libxml/parser.h>   /* C headers with their own extern "C" */
#include <libxml/xmlerror.h>
#include <zlib.h>

namespace xmltv {
namespace {
constexpr size_t kTitleMax = 240, kTextMax = 900;
constexpr uint64_t kInflatedMax = 3ull << 30;   /* a guide past 3 GB unpacked is not one to read on a console */

std::string trim(const std::string &s)
{
    const size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
} // namespace

struct Reader::Impl {
    ChannelFn on_channel;
    ProgrammeFn on_programme;
    std::string error;
    size_t count = 0;
    bool failed = false, started = false, root_checked = false;
    /* gzip */
    bool sniffed = false, gzip = false;
    std::string head;   /* the first bytes, until it's known which it is */
    z_stream z{};
    bool z_open = false;
    uint64_t inflated = 0;
    /* XML */
    xmlParserCtxtPtr ctxt = nullptr;
    xmlSAXHandler sax{};
    int depth = 0;
    bool in_channel = false, in_programme = false;
    std::string channel_id;
    std::vector<std::string> names;
    Programme prog;
    std::string sub_title;
    std::string *text = nullptr;   /* the element whose text is being kept */
    size_t text_max = 0;
    std::string capture;

    ~Impl()
    {
        if (ctxt) {
            if (ctxt->myDoc) xmlFreeDoc(ctxt->myDoc);
            xmlFreeParserCtxt(ctxt);
        }
        if (z_open) inflateEnd(&z);
    }

    static std::string attr_value(int n, const xmlChar **a, const char *name)
    {
        for (int i = 0; i < n; i++, a += 5)
            if (a[0] && std::strcmp((const char *)a[0], name) == 0 && a[3] && a[4])
                return std::string((const char *)a[3], (size_t)(a[4] - a[3]));
        return std::string();
    }
    void keep(std::string *target, size_t max)
    {
        capture.clear();
        text = target;
        text_max = max;
    }

    static void start_element(void *ctx, const xmlChar *name, const xmlChar *, const xmlChar *, int, const xmlChar **,
                              int nattr, int, const xmlChar **attrs)
    {
        auto &m = *static_cast<Impl *>(ctx);
        const char *n = (const char *)name;
        m.depth++;
        if (!m.root_checked) {
            m.root_checked = true;
            if (std::strcmp(n, "tv") != 0) {
                m.failed = true;
                m.error = "not an XMLTV guide";
                xmlStopParser(m.ctxt);
            }
            return;
        }
        if (m.depth == 2 && std::strcmp(n, "channel") == 0) {
            m.in_channel = true;
            m.channel_id = attr_value(nattr, attrs, "id");
            m.names.clear();
        } else if (m.depth == 2 && std::strcmp(n, "programme") == 0) {
            m.in_programme = true;
            m.prog = Programme();
            m.sub_title.clear();
            m.prog.channel = attr_value(nattr, attrs, "channel");
            m.prog.start = guide_text::xmltv_epoch(attr_value(nattr, attrs, "start"));
            m.prog.end = guide_text::xmltv_epoch(attr_value(nattr, attrs, "stop"));
        } else if (m.depth == 3 && m.in_channel && std::strcmp(n, "display-name") == 0) {
            m.names.emplace_back();
            m.keep(&m.names.back(), kTitleMax);
        } else if (m.depth == 3 && m.in_programme) {
            if (std::strcmp(n, "title") == 0 && m.prog.title.empty()) m.keep(&m.prog.title, kTitleMax);
            else if (std::strcmp(n, "sub-title") == 0 && m.sub_title.empty()) m.keep(&m.sub_title, kTitleMax);
            else if (std::strcmp(n, "desc") == 0 && m.prog.description.empty()) m.keep(&m.prog.description, kTextMax);
        }
    }
    static void end_element(void *ctx, const xmlChar *name, const xmlChar *, const xmlChar *)
    {
        auto &m = *static_cast<Impl *>(ctx);
        const char *n = (const char *)name;
        if (m.text && m.depth == 3) {
            *m.text = guide_text::clip(trim(m.capture), m.text_max);
            m.text = nullptr;
        }
        if (m.depth == 2 && m.in_channel && std::strcmp(n, "channel") == 0) {
            m.in_channel = false;
            if (!m.channel_id.empty() && m.on_channel) m.on_channel(m.channel_id, m.names);
        } else if (m.depth == 2 && m.in_programme && std::strcmp(n, "programme") == 0) {
            m.in_programme = false;
            if (!m.prog.channel.empty() && m.prog.start > 0 && !m.prog.title.empty()) {
                if (!m.sub_title.empty())
                    m.prog.description = m.prog.description.empty() ? m.sub_title : m.sub_title + " \xE2\x80\x93 " + m.prog.description;
                m.count++;
                if (m.on_programme) m.on_programme(std::move(m.prog));
            }
        }
        m.depth--;
    }
    static void characters(void *ctx, const xmlChar *ch, int len)
    {
        auto &m = *static_cast<Impl *>(ctx);
        if (m.text && len > 0 && m.capture.size() < m.text_max + 8)
            m.capture.append((const char *)ch, std::min((size_t)len, m.text_max + 8 - m.capture.size()));
    }
#if LIBXML_VERSION >= 21200
    static void on_error(void *ctx, const xmlError *e)
#else
    static void on_error(void *ctx, xmlErrorPtr e)
#endif
    {
        auto &m = *static_cast<Impl *>(ctx);
        if (e && e->level == XML_ERR_FATAL && m.error.empty() && e->message)
            m.error = trim(e->message);   /* recovered from; reported if nothing came of the file */
    }

    bool xml(const char *data, size_t n)
    {
        if (failed) return false;
        if (!ctxt) {
            static std::once_flag once;
            std::call_once(once, [] { xmlInitParser(); });
            sax.initialized = XML_SAX2_MAGIC;
            sax.startElementNs = start_element;
            sax.endElementNs = end_element;
            sax.characters = characters;
            sax.cdataBlock = characters;
            sax.serror = on_error;
            ctxt = xmlCreatePushParserCtxt(&sax, this, nullptr, 0, nullptr);
            if (!ctxt) {
                failed = true;
                error = "out of memory";
                return false;
            }
            xmlCtxtUseOptions(ctxt, XML_PARSE_RECOVER | XML_PARSE_NONET | XML_PARSE_NOCDATA | XML_PARSE_COMPACT);
        }
        while (n > 0 && !failed) {
            const int piece = (int)std::min(n, (size_t)(1 << 20));
            xmlParseChunk(ctxt, data, piece, 0);
            data += piece;
            n -= (size_t)piece;
        }
        return !failed;
    }

    bool bytes(const char *data, size_t n)
    {
        if (failed) return false;
        if (!sniffed) {   /* gzip starts 1f 8b; anything else is read as it is */
            head.append(data, n);
            if (head.size() < 2) return true;
            sniffed = true;
            gzip = (unsigned char)head[0] == 0x1f && (unsigned char)head[1] == 0x8b;
            std::string first;
            first.swap(head);
            if (gzip) {
                if (inflateInit2(&z, 15 + 32) != Z_OK) {
                    failed = true;
                    error = "zlib";
                    return false;
                }
                z_open = true;
            }
            return gzip ? unpack(first.data(), first.size()) : xml(first.data(), first.size());
        }
        return gzip ? unpack(data, n) : xml(data, n);
    }

    bool unpack(const char *data, size_t n)
    {
        static thread_local unsigned char out[1 << 16];
        z.next_in = (Bytef *)data;
        z.avail_in = (uInt)n;
        while (!failed) {
            z.next_out = out;
            z.avail_out = sizeof out;
            const int rc = inflate(&z, Z_NO_FLUSH);
            const size_t produced = sizeof out - z.avail_out;
            inflated += produced;
            if (inflated > kInflatedMax) {
                failed = true;
                error = "guide too large";
                return false;
            }
            if (produced && !xml((const char *)out, produced)) return false;
            if (rc == Z_STREAM_END) {
                if (z.avail_in == 0) return true;
                inflateReset(&z);   /* another gzip member follows */
                continue;
            }
            if (rc == Z_BUF_ERROR || (rc == Z_OK && z.avail_in == 0 && z.avail_out != 0)) return true;
            if (rc != Z_OK) {
                failed = true;
                error = "broken gzip data";
                return false;
            }
        }
        return false;
    }
};

Reader::Reader(ChannelFn channel, ProgrammeFn programme) : m(new Impl)
{
    m->on_channel = std::move(channel);
    m->on_programme = std::move(programme);
}
Reader::~Reader() = default;

bool Reader::feed(const char *data, size_t n) { return m->bytes(data, n); }

bool Reader::finish()
{
    if (!m->failed && !m->sniffed && !m->head.empty()) {   /* a tiny file */
        m->sniffed = true;
        std::string first;
        first.swap(m->head);
        m->xml(first.data(), first.size());
    }
    if (m->ctxt && !m->failed) xmlParseChunk(m->ctxt, nullptr, 0, 1);
    if (!m->failed && !m->root_checked) {
        m->failed = true;
        if (m->error.empty()) m->error = "not an XMLTV guide";
    }
    return !m->failed;
}

const std::string &Reader::error() const { return m->error; }
size_t Reader::programmes() const { return m->count; }

} // namespace xmltv
