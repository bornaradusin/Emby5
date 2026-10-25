/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The interface's language. The text in the code is Norwegian and is its own
 * key: T("Spill av") is "Spill av" in Norwegian, "Play" in English, "Reproducir"
 * in Spanish. A language's table falls back to English, and English to the
 * Norwegian (logged once), so nothing goes blank. English is in i18n.cpp, the
 * others in i18n_<code>.cpp.
 *
 * The PS5 console language is used. Unsupported values fall back to English.
 */
#pragma once

#include <string>

namespace i18n {

enum class Lang { English, Japanese, FrenchFrance, FrenchCanada, SpanishSpain, SpanishLatam, German, Italian, PortuguesePortugal, PortugueseBrazil, Dutch, Russian, Korean, ChineseSimplified, ChineseTraditional, Polish, Danish, Swedish, Norwegian, Finnish, Turkish, Arabic, Czech, Greek, Hungarian, Romanian, Thai, Indonesian, Vietnamese, Ukrainian, Croatian, SerbianCyrillic };
/* How many languages there are: tables indexed by Lang have exactly this many entries. */
constexpr int kLangCount = (int)Lang::SerbianCyrillic + 1;
/* Called once after system services initialize. No manual override exists. */
void use_ps5_system_language();
Lang lang();
/* Not Norwegian: what is outside the tables (dates, genre and language names)
 * is then English. */
inline bool english() { return lang() != Lang::Norwegian; }
/* Bumped on every change: screens that keep built text rebuild it. */
unsigned generation();

} // namespace i18n

/* The text in the interface's language (the argument is the Norwegian). */
const char *T(const char *nb);
inline std::string T(const std::string &nb) { return T(nb.c_str()); }
