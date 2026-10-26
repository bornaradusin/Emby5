/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The Live TV tab's TV guide: the channels of the group (or the search's) down
 * the left, the time across, a block per programme and a line at "now". The
 * focused programme's details are at the top. Up/Down change channel,
 * Left/Right move through its programmes, L1/R1 change group, X watches the
 * channel, Circle or Square go back to the channel cards. */
#include "ui/iptv.h"
#include "app/epg.h"
#include "app/i18n.h"
#include "app/iptv_m3u.h"
#include "gfx/art.h"
#include "platform/ime.h"
#include <algorithm>
#include <cmath>
#include <ctime>

namespace ui {
namespace {
constexpr float kChannelW = 330, kGridX = kPad + 350, kGridR = 1920 - kPad;
constexpr float kInfoTop = 70, kTimeY = 392, kRowsTop = 410, kRowH = 88, kCellH = 80;
constexpr float kBottom = 1080 - 96;
constexpr int64_t kSpan = 3 * 3600;          /* the grid shows three hours */
constexpr int64_t kHalf = 1800;
constexpr int64_t kAhead = 13 * 3600;        /* how far ahead Right can go */
constexpr float kPxPerSec = (kGridR - kGridX) / (float)kSpan;

int64_t now_s() { return (int64_t)std::time(nullptr); }
int64_t half_hour(int64_t t) { return t - ((t % kHalf) + kHalf) % kHalf; }

std::string clock(int64_t t)
{
    std::time_t tt = (std::time_t)t;
    std::tm tm{};
    localtime_r(&tt, &tm);
    char b[8];
    std::snprintf(b, sizeof b, "%02d:%02d", tm.tm_hour, tm.tm_min);
    return b;
}
bool same_day(int64_t a, int64_t b)
{
    std::time_t ta = (std::time_t)a, tb = (std::time_t)b;
    std::tm ma{}, mb{};
    localtime_r(&ta, &ma);
    localtime_r(&tb, &mb);
    return ma.tm_yday == mb.tm_yday && ma.tm_year == mb.tm_year;
}
int visible_rows() { return (int)((kBottom - kRowsTop) / kRowH); }

/* The programme on at t in a channel's list, or -1. */
int at(const epg::List &l, int64_t t)
{
    if (!l) return -1;
    for (size_t i = 0; i < l->size(); i++)
        if ((*l)[i].start <= t && t < (*l)[i].end) return (int)i;
    return -1;
}
} // namespace

void IPTV::open_guide()
{
    m_guide = true;
    m_guide_time = now_s();
    m_guide_left = half_hour(m_guide_time);
    m_guide_base = m_guide_left;
    m_guide_x.snap(0);
    const int first = std::max(0, m_index - visible_rows() / 2);
    m_guide_rows.snap((float)std::min(first, std::max(0, count() - visible_rows())));
    m_nav.snap(0);
    m_animating = true;
}

void IPTV::close_guide()
{
    m_guide = false;
    m_header = false;
    select_group(m_row, false);   /* back on the cards, on the same channel */
    remember();
    m_animating = true;
}

/* Keep the focused moment in the future-facing window, and the grid on it. */
void IPTV::guide_follow()
{
    const int64_t now = now_s();
    if (m_guide_time < now) m_guide_time = now;
    int64_t l = m_guide_left;
    if (m_guide_time < l + 600) l = half_hour(m_guide_time - 600);          /* the start in view, with room */
    if (m_guide_time > l + kSpan - 2700) l = half_hour(m_guide_time) - kHalf; /* moved past the right edge */
    m_guide_left = std::max(l, half_hour(now));                            /* never before this half hour */
    m_guide_x.to((float)(m_guide_left - m_guide_base));                    /* the base is when it opened */
    const int rows = visible_rows(), n = count();
    float first = m_guide_rows.target;
    if (m_index < first + 1) first = (float)std::max(0, m_index - 1);
    if (m_index > first + rows - 2) first = (float)(m_index - rows + 2);
    first = std::max(0.f, std::min(first, (float)std::max(0, n - rows)));
    m_guide_rows.to(first);
}

void IPTV::guide_move(int rows)
{
    const int n = count();
    if (n <= 0) return;
    const int to = m_index + rows;
    if (to < 0 || to >= n) {
        m_bump = true;
        return;
    }
    m_index = to;
    remember();
    guide_follow();
    m_animating = true;
}

Action IPTV::guide_input(uint32_t p)
{
    Action action;
    if (p & (NUVIO_BTN_CIRCLE | NUVIO_BTN_SQUARE)) {
        if ((p & NUVIO_BTN_CIRCLE) && !m_query.empty()) clear_search();
        else close_guide();
        return action;
    }
    if ((p & (NUVIO_BTN_L1 | NUVIO_BTN_R1)) && m_query.empty() && !m_groups->empty()) {
        const int to = m_row + ((p & NUVIO_BTN_R1) ? 1 : -1);
        if (to >= 0 && to < (int)m_groups->size()) {
            select_group(to, false);
            m_nav.snap(0);
            const int first = std::max(0, m_index - 1);
            m_guide_rows.snap((float)std::min(first, std::max(0, count() - visible_rows())));
            guide_follow();
        } else
            m_bump = true;
        return action;
    }
    if (p & NUVIO_BTN_UP) guide_move(-1);
    else if (p & NUVIO_BTN_DOWN) guide_move(1);
    else if (p & (NUVIO_BTN_LEFT | NUVIO_BTN_RIGHT)) {
        const auto *ch = channel(m_index);
        if (!ch) return action;
        const epg::Row row = epg::get(*ch);
        const int64_t now = now_s();
        const int cur = at(row.programmes, m_guide_time);
        if (p & NUVIO_BTN_RIGHT) {
            int64_t next = 0;
            if (row.programmes)
                for (const auto &pr : *row.programmes)
                    if (pr.start > m_guide_time) {
                        next = pr.start;
                        break;
                    }
            if (!next && (!row.programmes || row.programmes->empty()))
                next = half_hour(m_guide_time) + kHalf;   /* nothing known: by half hours */
            if (!next || next > now + kAhead) m_bump = true;
            else m_guide_time = next;
        } else if (cur >= 0) {   /* the one before, or the one on now; never the past */
            if ((*row.programmes)[cur].start <= now) m_bump = true;
            else m_guide_time = std::max(cur > 0 ? (*row.programmes)[cur - 1].start : now, now);
        } else if (m_guide_time <= now) {
            m_bump = true;
        } else {   /* in a gap, or nothing known: the last one that started before, else half an hour back */
            int64_t prev = half_hour(m_guide_time - 1);
            if (row.programmes)
                for (const auto &pr : *row.programmes)
                    if (pr.start < m_guide_time) prev = pr.start;
            m_guide_time = std::max(prev, now);
        }
        guide_follow();
        m_animating = true;
    } else if ((p & NUVIO_BTN_CROSS) && channel(m_index)) {
        size_t r = (size_t)m_row, c = (size_t)m_index;
        if (!m_query.empty()) {
            r = m_results[m_index].group;
            c = m_results[m_index].channel;
        }
        if (iptv_navigation::begin(m_groups, r, c)) action.kind = Action::PlayLiveTV;
    }
    return action;
}

void IPTV::draw_guide(double now_d, float dt)
{
    const int64_t now = now_s();
    m_guide_seen = epg::generation();
    m_guide_minute = now / 60;
    if (m_guide_time < now) m_guide_time = now;
    guide_follow();
    m_animating |= m_guide_rows.step(dt, 12.f);
    m_animating |= m_guide_x.step(dt, 10.f);
    const int n = count();
    m_index = std::clamp(m_index, 0, std::max(0, n - 1));
    const float left = (float)m_guide_base + m_guide_x.value;   /* the time at the grid's left edge */
    auto x_of = [&](int64_t t) { return kGridX + ((float)t - left) * kPxPerSec; };
    const auto &pal = theme_palette();
    const uint32_t accent = theme_focus_colour();

    gfx::fill({0, 0, gfx::W, gfx::H}, kBg);

    /* Which channels are on screen: fetch their guides, the focused one first. */
    const int first = std::max(0, (int)std::floor(m_guide_rows.value));
    const int last = std::min(n, first + visible_rows() + 1);
    {
        std::vector<iptv_xtream::Channel> wanted;
        if (const auto *f = channel(m_index)) wanted.push_back(*f);
        for (int i = std::max(0, first - 2); i < std::min(n, last + 2); i++)
            if (i != m_index)
                if (const auto *c = channel(i)) wanted.push_back(*c);
        epg::want(&m_client, wanted);
    }

    /* The header: where we are, and the group. */
    gfx::text(kPad, kInfoTop + 26, T("TV guide"), {gfx::SemiBold, 26}, kText3);
    std::string group = !m_query.empty() ? m_query : m_groups->empty() ? std::string() : (*m_groups)[m_row].title;
    if (m_query.empty() && m_groups->size() > 1) group = "L1 \xE2\x80\xB9  " + group + "  \xE2\x80\xBA R1";
    gfx::text(kGridR, kInfoTop + 26, group, {gfx::SemiBold, 26, 900}, kText2, 2);

    /* The focused programme. */
    const auto *focus_ch = channel(m_index);
    epg::Row focus_row;
    int focus = -1;
    if (focus_ch) {
        focus_row = epg::get(*focus_ch);
        focus = at(focus_row.programmes, m_guide_time);
    }
    if (focus_ch) {
        if (focus >= 0) {
            const epg::Programme &p = (*focus_row.programmes)[focus];
            gfx::text(kPad, kInfoTop + 92, p.title, {gfx::Bold, 44, kGridR - kPad}, kText);
            std::string when = clock(p.start) + " \xE2\x80\x93 " + clock(p.end);
            if (!same_day(p.start, now)) when = std::string(T("Tomorrow")) + " " + when;
            when += "  \xC2\xB7  " + focus_ch->name;
            if (p.start <= now && now < p.end)
                when += "  \xC2\xB7  " + std::to_string((p.end - now + 59) / 60) + T(" min left");
            gfx::text(kPad, kInfoTop + 136, when, {gfx::Medium, 24, kGridR - kPad}, kText2);
            if (p.start <= now && now < p.end && p.end > p.start) {
                const float w = 420, k = (float)(now - p.start) / (float)(p.end - p.start);
                gfx::fill({kPad, kInfoTop + 152, w, 6}, alpha(kText3, .35f), 3);
                gfx::fill({kPad, kInfoTop + 152, w * std::clamp(k, 0.f, 1.f), 6}, accent, 3);
            }
            if (!p.description.empty())
                gfx::text(kPad, kInfoTop + 200, p.description, {gfx::Regular, 24, kGridR - kPad, 3, 33}, kText2);
        } else {
            gfx::text(kPad, kInfoTop + 92, focus_ch->name, {gfx::Bold, 44, kGridR - kPad}, kText);
            const char *why = focus_row.loading ? T("Loading the TV guide \xE2\x80\xA6") : T("No programme information available");
            gfx::text(kPad, kInfoTop + 136, why, {gfx::Medium, 24, kGridR - kPad}, kText2);
        }
    } else {
        gfx::text(kPad, kInfoTop + 92, T("No channels available. Check your sources in Settings."), {gfx::Medium, 27, 1600}, kText2);
    }

    /* The time across, on the half hours. */
    gfx::push_scissor({kGridX, kTimeY - 30, kGridR - kGridX, 40});
    for (int64_t t = half_hour((int64_t)left); t < (int64_t)left + kSpan + kHalf; t += kHalf)
        gfx::text(x_of(t) + 8, kTimeY, clock(t), {gfx::SemiBold, 20}, t % 3600 ? kText3 : kText2);
    gfx::pop_scissor();
    gfx::fill({kGridX, kTimeY + 8, kGridR - kGridX, 1}, alpha(kText3, .4f));

    /* The rows. */
    gfx::push_scissor({0, kRowsTop - 10, gfx::W, kBottom - kRowsTop + 10});
    for (int i = first; i < last; i++) {
        const auto *ch = channel(i);
        if (!ch) continue;
        const float y = kRowsTop + ((float)i - m_guide_rows.value) * kRowH;
        const bool focused_row = i == m_index && m_focused && !m_menu.active();
        /* The channel: logo and name. */
        const gfx::Rect cell{kPad, y, kChannelW, kCellH};
        gfx::fill(cell, alpha(pal.panel, focused_row ? .95f : .7f), 12);
        bool drawn = false;
        if (!ch->logo.empty())
            if (const auto *image = art::get(ch->logo, 240, 135)) {
                const float w = (float)gfx::texture_width(image), h = (float)gfx::texture_height(image);
                if (w > 0 && h > 0) {
                    const float fit = std::min(104 / w, 60 / h);
                    gfx::image({kPad + 12 + (104 - w * fit) / 2, y + (kCellH - h * fit) / 2, w * fit, h * fit}, image, 1, 0, false);
                    drawn = true;
                }
            }
        gfx::text(kPad + (drawn ? 130 : 18), y + 48, ch->name, {gfx::SemiBold, 22, kChannelW - (drawn ? 142 : 30)},
                  focused_row ? kText : kText2);
        /* The programmes. */
        const epg::Row row = i == m_index ? focus_row : epg::get(*ch);
        gfx::push_scissor({kGridX - 10, y - 8, kGridR - kGridX + 20, kCellH + 16});   /* room for the focus ring */
        bool any = false;
        if (row.programmes)
            for (size_t k = 0; k < row.programmes->size(); k++) {
                const epg::Programme &p = (*row.programmes)[k];
                if (p.end <= (int64_t)left || p.start >= (int64_t)left + kSpan + kHalf) continue;
                const float x0 = std::max(kGridX, x_of(p.start)) + 2, x1 = std::min(kGridR, x_of(p.end)) - 2;
                if (x1 - x0 < 4) continue;
                any = true;
                const bool airing = p.start <= now && now < p.end;
                const bool focused = focused_row && (int)k == focus;
                const gfx::Rect r{x0, y, x1 - x0, kCellH};
                gfx::fill(r, focused ? alpha(accent, .32f) : alpha(pal.panel, airing ? .85f : .55f), 10);
                if (x1 - x0 > 30) {
                    gfx::text(x0 + 14, y + 34, p.title, {gfx::SemiBold, 21, x1 - x0 - 26}, focused || airing ? kText : kText2);
                    if (x1 - x0 > 90)
                        gfx::text(x0 + 14, y + 62, clock(p.start), {gfx::Medium, 18}, kText3);
                }
                if (focused) draw_focus_outline(r, 10);
            }
        if (!any) {   /* nothing known for these hours: one block, still the channel to watch */
            const gfx::Rect r{kGridX + 2, y, kGridR - kGridX - 4, kCellH};
            gfx::fill(r, alpha(pal.panel, .45f), 10);
            const char *text = row.loading ? T("Loading the TV guide \xE2\x80\xA6") : T("No programme information available");
            gfx::text(r.x + 14, y + 48, text, {gfx::Medium, 20, r.w - 28}, kText3);
            if (focused_row) draw_focus_outline(r, 10);
        }
        gfx::pop_scissor();
    }
    gfx::pop_scissor();
    if (n == 0 && !m_groups->empty())
        gfx::text(kPad, kRowsTop + 50, T("No matching channels. Triangle: edit search."), {gfx::Medium, 27, 1600}, kText2);

    /* Now: a line through the grid. */
    const float nx = x_of(now);
    if (nx >= kGridX && nx <= kGridR) {
        gfx::fill({nx - 1.5f, kTimeY + 2, 3, kBottom - kTimeY}, alpha(accent, .9f), 1.5f);
        gfx::fill({nx - 6, kTimeY - 2, 12, 12}, accent, 6);
    }

    /* The M3U guide's state, when it has something to say. */
    bool has_m3u = false;
    for (int i = first; i < last && !has_m3u; i++)
        if (const auto *c = channel(i)) has_m3u = c->source == "M3U";
    if (has_m3u) {
        const epg::Status st = epg::m3u_status();
        std::string note;
        if (!st.configured) note = T("For a guide to the M3U channels, add its address in Settings \xE2\x80\xBA Live TV.");
        else if (st.loading) note = T("Loading the TV guide \xE2\x80\xA6");
        else if (!st.error.empty()) note = T(st.error.c_str());
        if (!note.empty()) gfx::text(kGridR, gfx::H - 28, note, {gfx::Medium, 20, 760}, kText3, 2);
    }

    draw_pad_hints(kPad, gfx::H - 28,
                   {{PadButton::Cross, T("Watch")}, {PadButton::L1, ""}, {PadButton::R1, T("Change group")},
                    {PadButton::Square, T("Channel cards")}, {PadButton::Circle, T("Back")}},
                   0, 23);
    m_menu.draw();
    m_animating |= art::animating() || ime::active() || m_hold_dir;
    (void)now_d;
}

} // namespace ui
