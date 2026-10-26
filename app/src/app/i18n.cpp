/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "app/i18n.h"
#include "app/i18n_tables.h"

#include "evo_boot_trace.h"

#include <atomic>
#include <cstdio>

/* PS5 system language API, param 1 = console language. */
extern "C" int sceSystemServiceParamGetInt(int param_id, int *value);
#include <mutex>
#include <set>
#include <unordered_map>



namespace i18n {
namespace {

std::atomic<int> s_lang{(int)Lang::English};
std::atomic<unsigned> s_gen{0};

} // namespace

/* Language IDs returned by the PS5 system-language setting. */
static Lang from_ps5_language(int id)
{
    switch (id) {
    case 0: return Lang::Japanese;
    case 1: case 18: return Lang::English; /* US and UK */
    case 2: return Lang::FrenchFrance;
    case 22: return Lang::FrenchCanada;
    case 3: return Lang::SpanishSpain;
    case 20: return Lang::SpanishLatam;
    case 4: return Lang::German;
    case 5: return Lang::Italian;
    case 6: return Lang::Dutch;
    case 7: return Lang::PortuguesePortugal;
    case 17: return Lang::PortugueseBrazil;
    case 8: return Lang::Russian;
    case 9: return Lang::Korean;
    case 10: return Lang::ChineseTraditional;
    case 11: return Lang::ChineseSimplified;
    case 12: return Lang::Finnish;
    case 13: return Lang::Swedish;
    case 14: return Lang::Danish;
    case 15: return Lang::Norwegian;
    case 16: return Lang::Polish;
    case 19: return Lang::Turkish;
    case 21: return Lang::Arabic;
    case 23: return Lang::Czech;
    case 24: return Lang::Hungarian;
    case 25: return Lang::Greek;
    case 26: return Lang::Romanian;
    case 27: return Lang::Thai;
    case 28: return Lang::Vietnamese;
    case 29: return Lang::Indonesian;
    case 30: return Lang::Ukrainian;
    default: return Lang::English;
    }
}

namespace {
struct LanguageInfo { const char *code, *name; };
/* In the order of Lang. */
const LanguageInfo kLanguages[] = {
    {"en", "English"}, {"ja", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"}, {"fr", "Fran\xC3\xA7" "ais"},
    {"fr-CA", "Fran\xC3\xA7" "ais (Canada)"}, {"es", "Espa\xC3\xB1ol"}, {"es-419", "Espa\xC3\xB1ol (Latinoam\xC3\xA9rica)"},
    {"de", "Deutsch"}, {"it", "Italiano"}, {"pt", "Portugu\xC3\xAAs"}, {"pt-BR", "Portugu\xC3\xAAs (Brasil)"},
    {"nl", "Nederlands"}, {"ru", "\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9"},
    {"ko", "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4"}, {"zh-Hans", "\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87"},
    {"zh-Hant", "\xE7\xB9\x81\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87"}, {"pl", "Polski"}, {"da", "Dansk"},
    {"sv", "Svenska"}, {"nb", "Norsk"}, {"fi", "Suomi"}, {"tr", "T\xC3\xBCrk\xC3\xA7" "e"},
    {"ar", "\xD8\xA7\xD9\x84\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x8A\xD8\xA9"}, {"cs", "\xC4\x8C" "e\xC5\xA1tina"},
    {"el", "\xCE\x95\xCE\xBB\xCE\xBB\xCE\xB7\xCE\xBD\xCE\xB9\xCE\xBA\xCE\xAC"}, {"hu", "Magyar"},
    {"ro", "Rom\xC3\xA2n\xC4\x83"}, {"th", "\xE0\xB9\x84\xE0\xB8\x97\xE0\xB8\xA2"}, {"id", "Bahasa Indonesia"},
    {"vi", "Ti\xE1\xBA\xBFng Vi\xE1\xBB\x87t"}, {"uk", "\xD0\xA3\xD0\xBA\xD1\x80\xD0\xB0\xD1\x97\xD0\xBD\xD1\x81\xD1\x8C\xD0\xBA\xD0\xB0"},
    {"hr", "Hrvatski"}, {"sr", "\xD0\xA1\xD1\x80\xD0\xBF\xD1\x81\xD0\xBA\xD0\xB8"},
};
static_assert(sizeof(kLanguages) / sizeof(kLanguages[0]) == kLangCount, "one entry per i18n::Lang");

void set_lang(Lang chosen)
{
    const int new_lang = static_cast<int>(chosen);
    if (s_lang.exchange(new_lang) != new_lang) ++s_gen;
}
} // namespace

const char *language_code(Lang l) { return kLanguages[(int)l].code; }
const char *language_name(Lang l) { return kLanguages[(int)l].name; }

Lang ps5_language()
{
    int id = -1;
    /* Read only: no saved preference and no privilege/daemon interaction. */
    return sceSystemServiceParamGetInt(1, &id) == 0 ? from_ps5_language(id) : Lang::English;
}

void use_ps5_system_language()
{
    set_lang(ps5_language());
}

void use_language(const std::string &code)
{
    for (int i = 0; i < kLangCount; i++)
        if (!code.empty() && code == kLanguages[i].code) {
            set_lang((Lang)i);
            return;
        }
    use_ps5_system_language();
}

Lang lang() { return (Lang)s_lang.load(); }
unsigned generation() { return s_gen.load(); }

std::string fill(const std::string &text, std::initializer_list<std::pair<const char *, std::string>> values)
{
    std::string out;
    out.reserve(text.size() + 32);
    for (size_t i = 0; i < text.size();) {
        if (text[i] == '{') {
            const size_t end = text.find('}', i + 1);
            if (end != std::string::npos) {
                const std::string name = text.substr(i + 1, end - i - 1);
                bool found = false;
                for (const auto &v : values)
                    if (name == v.first) {
                        out += v.second;
                        found = true;
                        break;
                    }
                if (found) {
                    i = end + 1;
                    continue;
                }
            }
        }
        out += text[i++];
    }
    return out;
}

std::string date(const std::string &iso)
{
    int y = 0, m = 0, d = 0;
    if (std::sscanf(iso.c_str(), "%d-%d-%d", &y, &m, &d) != 3 || y < 1 || m < 1 || m > 12 || d < 1 || d > 31)
        return std::string();
    static const char *const months[] = {N_("January"), N_("February"), N_("March"), N_("April"),
                                         N_("May"), N_("June"), N_("July"), N_("August"),
                                         N_("September"), N_("October"), N_("November"), N_("December")};
    return fill(T("{month} {day}, {year}"),
                {{"month", T(months[m - 1])}, {"day", std::to_string(d)}, {"year", std::to_string(y)}});
}

} // namespace i18n

namespace i18n {

const char *find(const char *key)
{
    if (!key)
        return nullptr;
    const Lang l = lang();
    if (const Table *own = language_table(l)) {
        const auto it = own->find(key);
        if (it != own->end())
            return it->second;
    }
    if (l == Lang::Norwegian)
        return nullptr;
    const Table &en = english_table();
    const auto it = en.find(key);
    return it != en.end() ? it->second : nullptr;
}

std::string message(const std::string &text)
{
    if (text.empty())
        return text;
    if (const char *t = find(text.c_str()))
        return t;
    /* Messages with a number in them, as the catalogue code makes them. */
    static const char *const patterns[] = {N_("Catalogue connection failed ({code})"),
                                           N_("Catalogue read failed ({code})")};
    for (const char *pattern : patterns) {
        const std::string p = pattern;
        const size_t at = p.find("{code}");
        const std::string before = p.substr(0, at), after = p.substr(at + 6);
        if (text.size() > before.size() + after.size() && text.compare(0, before.size(), before) == 0 &&
            text.compare(text.size() - after.size(), after.size(), after) == 0)
            return fill(T(pattern), {{"code", text.substr(before.size(), text.size() - before.size() - after.size())}});
    }
    return text;
}

} // namespace i18n

const char *T(const char *key)
{
    if (!key)
        return key;
    if (const char *t = i18n::find(key))
        return t;
    /* Not in the tables: an English key is its own English, and a Norwegian
     * key its own Norwegian. Anything else is a text no table has yet. */
    const i18n::Lang l = i18n::lang();
    if (l == i18n::Lang::Norwegian || i18n::english_key(key))
        return key;
    static std::mutex lock;
    static std::set<std::string> missing;
    std::lock_guard<std::mutex> g(lock);
    if (missing.insert(key).second)
        evo_bt("i18n: no text for \"%s\"", key);
    return key;
}
