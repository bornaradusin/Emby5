/*
 * Nuvio PS5
 * Copyright (C) 2026 Husam Osman
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/*
 * Language codes and names for the player's track lists (from Nuvio's player
 * interface, nuvio_osd.cpp, which Emby5's PlayerUi replaced).
 */
#include "nuvio_osd.h"
#include "app/i18n.h"

#include <cctype>
#include <string>

std::string nuvio_language_key(const std::string &code)
{
    static const struct { const char *three, *two; } k[] = {
        {"eng", "en"}, {"spa", "es"}, {"fre", "fr"}, {"fra", "fr"}, {"ger", "de"}, {"deu", "de"},
        {"ita", "it"}, {"por", "pt"}, {"rus", "ru"}, {"jpn", "ja"}, {"kor", "ko"}, {"chi", "zh"},
        {"zho", "zh"}, {"ara", "ar"}, {"hin", "hi"}, {"tur", "tr"}, {"pol", "pl"}, {"dut", "nl"},
        {"nld", "nl"}, {"swe", "sv"}, {"nor", "no"}, {"nob", "nb"}, {"nno", "nn"}, {"dan", "da"}, {"fin", "fi"},
        {"gre", "el"}, {"ell", "el"}, {"heb", "he"}, {"cze", "cs"}, {"ces", "cs"}, {"hun", "hu"},
        {"rum", "ro"}, {"ron", "ro"}, {"bul", "bg"}, {"hrv", "hr"}, {"srp", "sr"}, {"slv", "sl"},
        {"slo", "sk"}, {"slk", "sk"}, {"ukr", "uk"}, {"tha", "th"}, {"vie", "vi"}, {"ind", "id"},
        {"may", "ms"}, {"msa", "ms"}, {"per", "fa"}, {"fas", "fa"}, {"urd", "ur"}, {"ben", "bn"},
        {"tam", "ta"}, {"tel", "te"}, {"mal", "ml"}, {"kan", "kn"}, {"mar", "mr"}, {"cat", "ca"},
        {"baq", "eu"}, {"eus", "eu"}, {"glg", "gl"}, {"ice", "is"}, {"isl", "is"}, {"est", "et"},
        {"lav", "lv"}, {"lit", "lt"}, {"fil", "tl"}, {"tgl", "tl"}, {"alb", "sq"}, {"sqi", "sq"},
        {"mac", "mk"}, {"mkd", "mk"}, {"bos", "bs"}, {"geo", "ka"}, {"kat", "ka"}, {"arm", "hy"},
        {"hye", "hy"}, {"aze", "az"}, {"kaz", "kk"}, {"uzb", "uz"}, {"mon", "mn"}, {"khm", "km"},
        {"lao", "lo"}, {"bur", "my"}, {"mya", "my"}, {"nep", "ne"}, {"sin", "si"}, {"swa", "sw"},
        {"afr", "af"}, {"amh", "am"}, {"wel", "cy"}, {"cym", "cy"}, {"gle", "ga"}, {"lat", "la"},
        {"pob", "pt-br"}, {"pt-BR", "pt-br"}, {"es-419", "es-419"},
    };
    std::string c;
    for (char ch : code)
        c += (char)(ch == '_' ? '-' : std::tolower((unsigned char)ch));
    for (const auto &e : k)
        if (c == e.three)
            return e.two;
    return c;
}

std::string nuvio_language_name(const std::string &code)
{
    static const struct { const char *k, *name; } n[] = {
        {"en", N_("English")}, {"es", N_("Spanish")}, {"es-419", N_("Spanish (Latin America)")},
        {"es-mx", N_("Spanish (Mexico)")}, {"fr", N_("French")}, {"fr-ca", N_("French (Canada)")},
        {"de", N_("German")}, {"it", N_("Italian")}, {"pt", N_("Portuguese")}, {"pt-br", N_("Portuguese (Brazil)")},
        {"pt-pt", N_("Portuguese (Portugal)")}, {"ru", N_("Russian")}, {"ja", N_("Japanese")}, {"ko", N_("Korean")},
        {"zh", N_("Chinese")}, {"zh-cn", N_("Chinese (Simplified)")}, {"zh-tw", N_("Chinese (Traditional)")},
        {"zh-hk", N_("Chinese (Hong Kong)")}, {"ar", N_("Arabic")}, {"hi", N_("Hindi")}, {"tr", N_("Turkish")},
        {"pl", N_("Polish")}, {"nl", N_("Dutch")}, {"sv", N_("Swedish")}, {"no", N_("Norwegian")}, {"nb", N_("Norwegian")}, {"nn", N_("Norwegian Nynorsk")},
        {"da", N_("Danish")}, {"fi", N_("Finnish")}, {"el", N_("Greek")}, {"he", N_("Hebrew")}, {"cs", N_("Czech")},
        {"hu", N_("Hungarian")}, {"ro", N_("Romanian")}, {"bg", N_("Bulgarian")}, {"hr", N_("Croatian")},
        {"sr", N_("Serbian")}, {"sl", N_("Slovenian")}, {"sk", N_("Slovak")}, {"uk", N_("Ukrainian")}, {"th", N_("Thai")},
        {"vi", N_("Vietnamese")}, {"id", N_("Indonesian")}, {"ms", N_("Malay")}, {"fa", N_("Persian")}, {"ur", N_("Urdu")},
        {"bn", N_("Bengali")}, {"ta", N_("Tamil")}, {"te", N_("Telugu")}, {"ml", N_("Malayalam")}, {"kn", N_("Kannada")},
        {"mr", N_("Marathi")}, {"ca", N_("Catalan")}, {"eu", N_("Basque")}, {"gl", N_("Galician")}, {"is", N_("Icelandic")},
        {"et", N_("Estonian")}, {"lv", N_("Latvian")}, {"lt", N_("Lithuanian")}, {"tl", N_("Filipino")},
        {"sq", N_("Albanian")}, {"mk", N_("Macedonian")}, {"bs", N_("Bosnian")}, {"ka", N_("Georgian")},
        {"hy", N_("Armenian")}, {"az", N_("Azerbaijani")}, {"kk", N_("Kazakh")}, {"uz", N_("Uzbek")}, {"mn", N_("Mongolian")},
        {"km", N_("Khmer")}, {"lo", N_("Lao")}, {"my", N_("Burmese")}, {"ne", N_("Nepali")}, {"si", N_("Sinhala")},
        {"sw", N_("Swahili")}, {"af", N_("Afrikaans")}, {"am", N_("Amharic")}, {"cy", N_("Welsh")}, {"ga", N_("Irish")},
        {"la", N_("Latin")}, {"und", ""}, {"unknown", ""}, {"mul", N_("Multiple")}, {"zxx", ""},
    };
    const std::string key = nuvio_language_key(code);
    for (const auto &e : n)
        if (key == e.k)
            return e.name;
    /* "en-gb" and similar: the base language. */
    const size_t dash = key.find('-');
    if (dash != std::string::npos) {
        const std::string base = key.substr(0, dash);
        for (const auto &e : n)
            if (base == e.k)
                return e.name;
    }
    if (code.empty())
        return "";
    std::string out = code;
    if (out.size() <= 3)
        for (auto &ch : out)
            ch = (char)std::toupper((unsigned char)ch);
    return out;
}
