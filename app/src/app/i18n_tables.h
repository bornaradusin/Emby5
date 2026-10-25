/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The translation tables besides English (i18n.cpp), each keyed by the
 * Norwegian text: i18n_es.cpp, i18n_fr.cpp, i18n_de.cpp, i18n_pt.cpp, i18n_it.cpp,
 * i18n_zh.cpp; the menu words of the other languages are in i18n_extra.cpp.
 */
#pragma once

#include <string>
#include <unordered_map>

namespace i18n {
const std::unordered_map<std::string, const char *> &extra_table(int language);

const std::unordered_map<std::string, const char *> &spanish_table();
const std::unordered_map<std::string, const char *> &french_table();
const std::unordered_map<std::string, const char *> &german_table();
const std::unordered_map<std::string, const char *> &portuguese_table();
const std::unordered_map<std::string, const char *> &italian_table();
const std::unordered_map<std::string, const char *> &chinese_simplified_table();
const std::unordered_map<std::string, const char *> &chinese_traditional_table();

} // namespace i18n
