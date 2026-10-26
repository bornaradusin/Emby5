/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Which table each language uses (i18n_tables.h).
 */
#include "app/i18n_tables.h"

namespace i18n {

const Table &table_ja();
const Table &table_fr();
const Table &table_es();
const Table &table_de();
const Table &table_it();
const Table &table_pt();
const Table &table_nl();
const Table &table_ru();
const Table &table_ko();
const Table &table_zh_hans();
const Table &table_zh_hant();
const Table &table_pl();
const Table &table_da();
const Table &table_sv();
const Table &table_nb();
const Table &table_fi();
const Table &table_tr();
const Table &table_ar();
const Table &table_cs();
const Table &table_el();
const Table &table_hu();
const Table &table_ro();
const Table &table_th();
const Table &table_id();
const Table &table_vi();
const Table &table_uk();
const Table &table_hr();
const Table &table_sr();

const Table *language_table(Lang lang)
{
    switch (lang) {
    case Lang::Japanese:
        return &table_ja();
    case Lang::FrenchFrance:
    case Lang::FrenchCanada:
        return &table_fr();
    case Lang::SpanishSpain:
    case Lang::SpanishLatam:
        return &table_es();
    case Lang::German:
        return &table_de();
    case Lang::Italian:
        return &table_it();
    case Lang::PortuguesePortugal:
    case Lang::PortugueseBrazil:
        return &table_pt();
    case Lang::Dutch:
        return &table_nl();
    case Lang::Russian:
        return &table_ru();
    case Lang::Korean:
        return &table_ko();
    case Lang::ChineseSimplified:
        return &table_zh_hans();
    case Lang::ChineseTraditional:
        return &table_zh_hant();
    case Lang::Polish:
        return &table_pl();
    case Lang::Danish:
        return &table_da();
    case Lang::Swedish:
        return &table_sv();
    case Lang::Norwegian:
        return &table_nb();
    case Lang::Finnish:
        return &table_fi();
    case Lang::Turkish:
        return &table_tr();
    case Lang::Arabic:
        return &table_ar();
    case Lang::Czech:
        return &table_cs();
    case Lang::Greek:
        return &table_el();
    case Lang::Hungarian:
        return &table_hu();
    case Lang::Romanian:
        return &table_ro();
    case Lang::Thai:
        return &table_th();
    case Lang::Indonesian:
        return &table_id();
    case Lang::Vietnamese:
        return &table_vi();
    case Lang::Ukrainian:
        return &table_uk();
    case Lang::Croatian:
        return &table_hr();
    case Lang::SerbianCyrillic:
        return &table_sr();
    case Lang::English:
        break;
    }
    return nullptr;
}

bool english_key(const char *key)
{
    return key && table_nb().count(key);   /* Norwegian has every English key */
}

} // namespace i18n
