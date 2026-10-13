/* Emby5 - GPL-3.0-or-later. Local, transactional appearance editing. */
#include "ui/settings_screen.h"
#include "ui/settings_layout.h"
#include "ui/theme_presets.h"
#include "platform/ime.h"
#include "nuvio_input.h"
#include <algorithm>
#include <array>

namespace ui {
namespace {
const char *const groups[]={"Themes & Colours","Buttons & Panels","Background","Text & Browsing Layout",
    "Motion & Reading","Content Display","Preview","Saved Custom Themes","Apply & Recovery"};
const char *const summaries[]={"Presets and coordinated palettes","Shape, material and matching focus",
    "Artwork, contrast and dimming","Readable text and comfortable browsing","Animations and complete descriptions",
    "Spoilers, title logos and detail posters","Compare four lightweight sample scenes",
    "Three named slots; saved when you Apply","Save, discard or safely restore defaults"};
constexpr int group_count=sizeof(groups)/sizeof(groups[0]);
// Temporary drawing policy. Nothing is saved; the caller's draft is restored.
struct ScopedPreview {
    settings::Appearance previous=settings::get_appearance();
    float scale;
    explicit ScopedPreview(const settings::Appearance &a):scale(gfx::exchange_text_scale(a.ui_text_percent/100.f)){
        settings::preview_appearance(&a);
    }
    ~ScopedPreview(){settings::preview_appearance(&previous);gfx::exchange_text_scale(scale);}
};
}

void SettingsScreen::begin_appearance(){
    m_appearance.begin(settings::get().local);settings::preview_appearance(&m_appearance.draft);
    m_appearance_note.clear();m_appearance_page=-1;m_appearance_index=m_appearance_root_index=0;
    m_preview_scene=0;m_compare_appearance=false;m_appearance_confirm=ConfirmNone;
    m_section=4;m_row=Theme;m_scroll.snap(0);m_drop.hide();
}
void SettingsScreen::end_appearance(bool apply){
    if(m_appearance.active&&apply)settings::set_local(m_appearance.apply_to(settings::get().local));
    settings::preview_appearance(nullptr);m_appearance.end();m_appearance_confirm=ConfirmNone;
    m_section=7;m_row=Appearance;m_scroll.snap(0);m_drop.hide();
}
std::vector<SettingsScreen::Row> SettingsScreen::appearance_rows() const {
    switch(m_appearance_page){
    case 0:return {Theme,ColourVariant,Accent};
    case 1:return {ButtonShape,ControlStyle,FocusOutline};
    case 2:return {Background,BackgroundArt,BackdropDim};
    case 3:return {UiTextSize,PosterSize};
    case 4:return {PosterEnlarge,ReducedMotion,DescriptionScroll};
    case 5:return {SpoilerProtection,TitleStyle,DetailPoster};
    case 6:return {PreviewScene,CompareAppearance};
    case 7:return {CustomSlot,SaveMyTheme,LoadMyTheme,RenameMyTheme};
    case 8:return {ApplyAppearance,CancelAppearance,RestoreAppearance,ResetAppearance};
    default:return {};
    }
}
std::string SettingsScreen::appearance_help(Row row) const {
    switch(row){
    case Theme:return "Presets reset the draft overrides, not your saved custom slots. Apply to keep changes.";
    case ColourVariant:return "A coordinated palette for panels, accents and selected controls. Explicit overrides still win.";
    case Accent:return "Changes accents without sacrificing text contrast. The active outline always follows the control shape.";
    case ButtonShape:return "Applies to buttons and tabs, not posters or avatars. Border, highlight and shadow share one shape.";
    case ControlStyle:return "Solid and Glass use the same geometry. Compact glass uses a clean sheen, not magnified background pixels.";
    case FocusOutline:return "Strong makes the matching selection outline thicker. The selected control stays visible in every theme.";
    case Background:return "Choose a neutral or coloured browsing background. Panels stay coordinated. Video brightness, HDR and subtitles are unchanged.";
    case BackgroundArt:return "Choose title artwork or a plain background on browsing pages.";
    case BackdropDim:return "Dims artwork behind the interface. Video playback is not affected.";
    case UiTextSize:return "Changes interface text; subtitle size is still controlled separately under Playback.";
    case PosterSize:return "Adjusts browsing density and its matching navigation grid.";
    case PosterEnlarge:return "Turn zoom off without removing the selection outline.";
    case ReducedMotion:return "Stops nonessential motion and automatic description scrolling; Read more remains available.";
    case DescriptionScroll:return "The full synopsis is always available through Read more, even when automatic movement is off.";
    case SpoilerProtection:return "Hide unwatched-episode descriptions and thumbnails. Revealing a spoiler never marks it watched.";
    case TitleStyle:return "Use the server logo or plain text. Text is always the fallback when a logo is unavailable.";
    case DetailPoster:return "Show the poster beside the movie/series description, or keep a backdrop-only layout.";
    case PreviewScene:return "Sample scenes use ordinary UI geometry only. No video decoder, stream or artwork download is opened.";
    case CompareAppearance:return "Compare the sample on the right with the appearance applied before you opened this editor.";
    case CustomSlot:return "Choose one of three saved slots. Slot names and contents are only persisted when you Apply.";
    case SaveMyTheme:return "Capture the current draft. A populated slot asks before it is overwritten.";
    case LoadMyTheme:return "Load into preview first; your currently applied appearance is unchanged until Apply.";
    case RenameMyTheme:return "Give this slot a short name. Names can be changed without replacing its saved appearance.";
    case ApplyAppearance:return "Save the current draft and custom slots, then return to System.";
    case CancelAppearance:return "Apply, discard or keep editing. The confirmation follows your draft theme and button shape.";
    case RestoreAppearance:return "Put the draft back to your last applied look. Account and playback settings are never reset.";
    case ResetAppearance:return "Reset only the selected preset's appearance defaults. Saved custom slots remain intact.";
    default:return {};
    }
}
Action SettingsScreen::input_appearance(uint32_t p){
    if(m_appearance_confirm!=ConfirmNone){
        const int count=m_appearance_confirm==ConfirmExit?3:2;
        if(p&NUVIO_BTN_CIRCLE){m_appearance_confirm=ConfirmNone;return {};}
        if(p&(NUVIO_BTN_UP|NUVIO_BTN_LEFT))m_confirmation_choice=std::max(0,m_confirmation_choice-1);
        else if(p&(NUVIO_BTN_DOWN|NUVIO_BTN_RIGHT))m_confirmation_choice=std::min(count-1,m_confirmation_choice+1);
        else if(p&NUVIO_BTN_CROSS){
            const auto kind=m_appearance_confirm;const auto choice=m_confirmation_choice;m_appearance_confirm=ConfirmNone;
            if(kind==ConfirmExit){if(choice<2)end_appearance(choice==0);}
            else if(choice==0){
                if(kind==ConfirmOverwrite){m_appearance.remember();m_appearance_note="Slot updated in draft. Apply saves it.";}
                else {m_appearance.reset();m_appearance_note="Preset defaults restored in draft.";}
                settings::preview_appearance(&m_appearance.draft);
            }
        }
        return {};
    }
    if(p&NUVIO_BTN_OPTIONS){end_appearance(true);return {};}
    const auto request_exit=[&]{
        if(!m_appearance.dirty()){end_appearance(false);return;}
        m_appearance_confirm=ConfirmExit;m_confirmation_choice=2; // safe default: keep editing
    };
    if(p&NUVIO_BTN_CIRCLE){
        if(m_appearance_page>=0){m_appearance_page=-1;m_appearance_index=m_appearance_root_index;}
        else request_exit();
        return {};
    }
    auto rows=appearance_rows();const int count=m_appearance_page<0?group_count:(int)rows.size();
    if(p&NUVIO_BTN_DOWN)m_appearance_index=std::min(count-1,m_appearance_index+1);
    else if(p&NUVIO_BTN_UP)m_appearance_index=std::max(0,m_appearance_index-1);
    if(m_appearance_page<0){
        if(p&NUVIO_BTN_CROSS){m_appearance_root_index=m_appearance_index;m_appearance_page=m_appearance_index;
            m_appearance_index=0;rows=appearance_rows();m_row=rows.front();}
        return {};
    }
    m_appearance_index=std::max(0,std::min((int)rows.size()-1,m_appearance_index));
    m_row=rows[m_appearance_index];
    if(p&(NUVIO_BTN_LEFT|NUVIO_BTN_RIGHT)){change((Row)m_row,p&NUVIO_BTN_RIGHT?1:-1);return {};}
    if(!(p&NUVIO_BTN_CROSS))return {};
    switch((Row)m_row){
    case ApplyAppearance:end_appearance(true);break;
    case CancelAppearance:request_exit();break;
    case RestoreAppearance:m_appearance.restore();settings::preview_appearance(&m_appearance.draft);
        m_appearance_note="Last applied appearance restored in draft.";break;
    case ResetAppearance:m_appearance_confirm=ConfirmReset;m_confirmation_choice=1;break;
    case SaveMyTheme:
        if(m_appearance.slots[m_appearance.selected_slot].valid){m_appearance_confirm=ConfirmOverwrite;m_confirmation_choice=1;}
        else {m_appearance.remember();m_appearance_note="Custom theme captured. Apply saves it.";}break;
    case LoadMyTheme:
        m_appearance.recall();settings::preview_appearance(&m_appearance.draft);
        m_appearance_note=m_appearance.saved_valid?"Custom theme loaded into draft.":"This slot is empty.";break;
    case RenameMyTheme:{
        const int slot=m_appearance.selected_slot;
        ime::request(ime::Kind::Text,"Custom theme name",m_appearance.slots[slot].name,[this,slot](const std::string &name){
            if(!m_appearance.active)return;
            m_appearance.slots[slot].name=settings::theme_slot_name(name);
            m_appearance_note="Name updated in draft. Apply saves it.";
        });break;
    }
    default:change((Row)m_row,1);break;
    }
    return {};
}

void SettingsScreen::draw_appearance_confirmation(){
    if(m_appearance_confirm==ConfirmNone)return;
    // Use the current draft even if the sample is comparing the last applied
    // appearance. Palette contrast protection and safe default actions remain.
    ScopedPreview scoped(m_appearance.draft);
    const auto palette = theme_palette();
    const float scale = theme_text_scale();
    const bool exit = m_appearance_confirm == ConfirmExit;
    const int count = exit ? 3 : 2;
    const float row_h = 66.f * scale, gap = 16.f;
    const float header_h = 164.f * scale;
    const float box_h = 72.f + header_h + count * row_h + (count - 1) * gap;
    const gfx::Rect box{410.f, (gfx::H - box_h) * .5f, 1100.f, box_h};
    gfx::fill({0, 0, gfx::W, gfx::H}, 0xb8000000u);
    // Opaque theme-coloured backing makes even Glass readable over artwork.
    // It does not replace the theme with the old hard-coded blue recovery skin.
    gfx::fill_shape(theme_surface_shape(box, 24, false), palette.panel);
    glass_panel(box, 24, 1.f, true);
    const float left = box.x + 42.f, width = box.w - 84.f;
    const char *title = exit ? "Unsaved appearance changes" : m_appearance_confirm == ConfirmOverwrite ?
        "Replace this custom theme?" : "Reset this preset?";
    const char *desc = exit ? "Apply your draft, discard it, or keep editing." : m_appearance_confirm == ConfirmOverwrite ?
        "The selected slot will change in this draft. Apply saves it; Discard keeps the original." :
        "Only appearance overrides will reset. Saved slots, accounts and playback settings stay unchanged.";
    gfx::text(left, box.y + 36.f + 31.f * scale, title, {gfx::Bold, 31, width}, palette.text);
    gfx::text(left, box.y + 48.f + 70.f * scale, desc, {gfx::Regular, 24, width, 2, 32},
              theme_secondary_colour(palette));
    const char *choices[] = {exit ? "Apply changes" : m_appearance_confirm == ConfirmOverwrite ?
        "Replace slot" : "Reset to preset defaults", exit ? "Discard changes" : "Keep editing", "Keep editing"};
    for (int i = 0; i < count; ++i) {
        const gfx::Rect button{left, box.y + 36.f + header_h + i * (row_h + gap), width, row_h};
        const bool focused = i == m_confirmation_choice;
        glass_panel(button, 16, 1.f, false, focused ? 1.f : 0.f);
        if (focused) draw_focus_outline(button, 16);
        const auto shape = theme_surface_shape(button, 16, true);
        const float pad = std::max(24.f, shape.corner * .8f);
        gfx::text(button.x + pad, button.y + button.h * .5f + 8.f * scale, choices[i],
                  {gfx::SemiBold, 25, button.w - 2.f * pad}, palette.text);
    }
}

void SettingsScreen::draw_appearance_preview(){
    const auto look=m_compare_appearance?m_appearance.original:m_appearance.draft;
    ScopedPreview scoped(look);
    const auto p=theme_palette();const gfx::Rect area{1180,274,620,556};
    gfx::fill(area,p.page,18);
    // A calm, cheap sample backdrop; no offscreen allocation/stream/texture cache.
    if(look.background_artwork){gfx::fill_vgradient(area,p.accent,p.panel,18);
        gfx::fill(area,alpha(p.page,theme_backdrop_dim(.84f)),18);}
    gfx::stroke_shape({area,18,false},1,theme_secondary_colour(p,true));
    gfx::text(area.x+26,area.y+40,m_compare_appearance?"Preview - last applied":"Preview - current draft",{gfx::SemiBold,22,area.w-52},p.text);
    const float x=area.x+28,y=area.y+75,w=area.w-56;
    const auto button=[&](gfx::Rect b,const char *label,bool focus){
        glass_panel(b,16,1,false,focus?1.f:0.f);if(focus)draw_focus_outline(b,16);
        const float pad=theme_surface_shape(b,16,true).corner>=b.h*.45f?b.h*.32f:18.f;
        gfx::text(b.x+pad,b.y+b.h*.5f+9,label,{gfx::SemiBold,23,b.w-2*pad},p.text);
    };
    const auto poster=[&](gfx::Rect b,int n,bool focus){
        gfx::fill_vgradient(b,p.accent,p.panel,12);
        gfx::text(b.x+b.w/2,b.y+b.h*.56f,std::to_string(n),{gfx::Bold,30,b.w-10},p.text,1);
        if(focus)draw_image_focus_outline(b,12);
    };
    if(m_preview_scene==0){
        button({x,y,w,64},"Navigation tab",false);
        button({x,y+92,w,74},"Selected button",true);
        button({x,y+190,w-96,70},"Ordinary button",false);
        button({x+w-78,y+190,78,70},"+",false);
        gfx::fill({x,y+300,80,80},p.panel,40);
        gfx::text(x+40,y+350,"A",{gfx::Bold,28},p.text,1);
        gfx::text(x+103,y+332,"Avatars stay round",{gfx::Medium,22,w-108},p.text);
        gfx::text(x+103,y+367,theme_shape_name(look.button_shape),{gfx::Regular,20,w-108},theme_secondary_colour(p));
    } else if(m_preview_scene==1){
        const int columns=look.poster_size==0?4:look.poster_size==2?2:3;
        const float pw=(w-(columns-1)*18)/columns,ph=pw*1.5f;
        const int row_count=2*(ph+39)<=area.h-75?2:1;
        for(int row=0;row<row_count;++row)for(int col=0;col<columns;++col){
            gfx::Rect b{x+col*(pw+18),y+row*(ph+39),pw,ph};poster(b,row*columns+col+1,!row&&!col);
            gfx::text(b.x,b.y+b.h+27,"Sample title",{gfx::Medium,19,pw},p.text);
        }
    } else if(m_preview_scene==2){
        float tx=x;
        if(look.detail_poster){poster({x,y,98,147},1,false);tx+=122;}
        gfx::text(tx,y+40,look.title_as_text?"Sample movie":"SAMPLE MOVIE",{gfx::Bold,28,w-(tx-x)},p.text);
        gfx::text(tx,y+80,"Film / Series",{gfx::Medium,21,w-(tx-x)},theme_secondary_colour(p));
        gfx::text(x,y+196,look.spoiler_protection?"Unwatched episode details hidden. Reveal never changes watched status.":
            "A complete description remains available through Read more. This is lightweight sample text, not an active video.",
            {gfx::Regular,23,w,4,33},p.text);
        button({x,y+337,w,70},"Read more",true);
    } else {
        gfx::fill({x,y,w,212},0xff080c14u,12);
        gfx::text(x+24,y+111,"Sample playback controls",{gfx::Medium,23,w-48},0xfff3f7ffu);
        gfx::fill({x+18,y+236,w-36,5},p.panel,2);
        gfx::fill({x+18,y+236,(w-36)*.45f,5},p.accent,2);
        button({x,y+285,w*.57f,68},"Play / Pause",true);
        button({x+w*.61f,y+285,w*.39f,68},"Audio",false);
        gfx::text(x,y+405,"No video or decoder is opened by this preview.",{gfx::Regular,21,w,2,30},p.text);
    }
}

void SettingsScreen::draw_appearance(float dt){
    gfx::fill({0,0,gfx::W,gfx::H},kBg);
    std::string path="Settings / System / Appearance";
    if(m_appearance_page>=0)path+=" / "+std::string(groups[m_appearance_page]);
    gfx::text(120,179,path,{gfx::Bold,32,1680},kText);
    gfx::text(120,222,std::string(theme_name(m_appearance.draft.theme))+" - "+m_appearance.status(),{gfx::Medium,22,1680},kText2);
    const auto rows=appearance_rows();const int count=m_appearance_page<0?group_count:(int)rows.size();
    constexpr int visible=6;
    const int first=std::min(std::max(0,m_appearance_index-visible+1),std::max(0,count-visible));
    const float left=120,width=1015,row_h=94,top=274;
    // Whole focused rows are kept inside this viewport, including outer strokes.
    gfx::push_scissor({left-12,top-12,width+24,visible*row_h+12});
    for(int i=first;i<count&&i<first+visible;++i){
        const bool selected=i==m_appearance_index;
        const gfx::Rect box{left,top+(i-first)*row_h,width,80};
        glass_panel(box,16,1.f,false,selected?1.f:0.f);
        if(selected)draw_focus_outline(box,16);
        const auto shape=theme_surface_shape(box,16,true);
        const float pad=std::max(24.f,shape.corner*.8f);
        const std::string label=m_appearance_page<0?groups[i]:settings_layout::label_of(rows[i]);
        const std::string val=m_appearance_page<0?summaries[i]:value(rows[i]);
        gfx::text(box.x+pad,box.y+30,label,{gfx::SemiBold,22,width-2*pad},kText);
        gfx::text(box.x+pad,box.y+62,val,{gfx::Medium,20,width-2*pad},kText2);
    }
    gfx::pop_scissor();
    if(count>visible){
        const float track=visible*row_h-14,thumb=track*visible/count;
        gfx::fill({1153,top,4,track},alpha(kText,.2f),2);
        gfx::fill({1151,top+(track-thumb)*first/(count-visible),8,thumb},theme_focus_colour(),4);
    }
    const std::string hint=m_appearance_page<0?"Choose a section. Your current look stays saved until you Apply.":appearance_help(rows[m_appearance_index]);
    gfx::text(120,879,hint,{gfx::Regular,21,1015,2,28},kText2);
    draw_appearance_preview();
    gfx::text(120,958,"Up/Down: navigate   Left/Right: change   X: select   Options: Apply   Circle: Back / Cancel",{gfx::Medium,21,1680},kText);
    gfx::text(120,1004,m_appearance_note.empty()?"Draft preview. Apply saves; leaving with changes asks before discarding.":m_appearance_note,{gfx::Regular,20,1680},kText2);
    draw_appearance_confirmation();(void)dt;
}
} // namespace ui
