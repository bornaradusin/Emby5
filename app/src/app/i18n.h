/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The interface's language. A text in the code is its own key: the older
 * texts are Norwegian (T("Spill av") is "Play" in English, "Reproducir" in
 * Spanish), the newer ones English. The tables are i18n_<code>.cpp: i18n_en.cpp
 * has English for the Norwegian keys, i18n_nb.cpp Norwegian for the English
 * keys, and every other language both kinds of key. A text no table has stays
 * as its key (logged once), so nothing goes blank.
 *
 * The language is the one chosen in Settings › System › Language, or the
 * PS5's when none is chosen; a PS5 language Emby5 doesn't have is English.
 */
#pragma once

#include <initializer_list>
#include <string>
#include <utility>

namespace i18n {

enum class Lang { English, Japanese, FrenchFrance, FrenchCanada, SpanishSpain, SpanishLatam, German, Italian, PortuguesePortugal, PortugueseBrazil, Dutch, Russian, Korean, ChineseSimplified, ChineseTraditional, Polish, Danish, Swedish, Norwegian, Finnish, Turkish, Arabic, Czech, Greek, Hungarian, Romanian, Thai, Indonesian, Vietnamese, Ukrainian, Croatian, SerbianCyrillic };
/* How many languages there are: tables indexed by Lang have exactly this many entries. */
constexpr int kLangCount = (int)Lang::SerbianCyrillic + 1;
/* The PS5's own language (system services must be up). */
Lang ps5_language();
void use_ps5_system_language();
/* code from language_code() picks that language; an empty or unknown one
 * follows the PS5 (Settings › System › Language). */
void use_language(const std::string &code);
/* A language's code ("en", "fr-CA", "zh-Hans", "hr" ...) and its name in itself ("Hrvatski"). */
const char *language_code(Lang lang);
const char *language_name(Lang lang);
Lang lang();
/* Bumped on every change: screens that keep built text rebuild it. */
unsigned generation();
/* text with its {name} placeholders filled in, so each language can put them
 * where its word order needs them: fill(T("Every {hours} hours"), {{"hours", "12"}}). */
std::string fill(const std::string &text, std::initializer_list<std::pair<const char *, std::string>> values);
/* A date from an ISO one ("2026-10-25...") in the interface's language, as
 * "October 25, 2026" or "25. oktober 2026"; empty when iso isn't a date. */
std::string date(const std::string &iso);
/* key in the interface's language, or nullptr when no table has it. */
const char *find(const char *key);
/* A status or error kept as data in English (the code compares some of them),
 * in the interface's language when it is a known text, else as it is. */
std::string message(const std::string &text);

} // namespace i18n

/* The text in the interface's language. The argument is the text's key: the
 * Norwegian for the older texts, the English for the newer ones (both kinds
 * are in every language's table, and Norwegian has one for the English keys). */
const char *T(const char *key);
inline std::string T(const std::string &key) { return T(key.c_str()); }
/* Marks a key that is kept as it is here and translated where it is shown,
 * with T(): a label in a table of choices, say. */
#define N_(key) key
/* T with {name} placeholders filled in (i18n::fill). */
inline std::string TF(const char *key, std::initializer_list<std::pair<const char *, std::string>> values)
{
    return i18n::fill(T(key), values);
}
