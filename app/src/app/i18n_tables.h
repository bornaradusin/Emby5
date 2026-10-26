/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The translation tables, one per language in i18n_<code>.cpp, made from one
 * list so every language has every text: i18n_en.cpp is English for the
 * Norwegian keys, i18n_nb.cpp Norwegian for the English keys, and the other
 * languages have both kinds of key.
 */
#pragma once

#include "app/i18n.h"

#include <string>
#include <unordered_map>

namespace i18n {

using Table = std::unordered_map<std::string, const char *>;

/* Norwegian key -> English. */
const Table &english_table();
/* The table of lang, or nullptr for English (whose English keys are their own text). */
const Table *language_table(Lang lang);
/* Whether key is one of the English keys. */
bool english_key(const char *key);

} // namespace i18n
