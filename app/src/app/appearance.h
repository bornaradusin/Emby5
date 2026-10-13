/* Emby5 - GPL-3.0-or-later. Pure appearance/editor policy shared with tests. */
#pragma once
#include "app/settings.h"
#include <algorithm>
namespace settings {
inline Appearance normalize_appearance(Appearance a) {
    auto clamp=[](int n,int lo,int hi){return std::max(lo,std::min(hi,n));};
    a.theme=clamp(a.theme,0,kThemePresetCount-1); a.colour_variant=clamp(a.colour_variant,0,5);
    a.button_shape=clamp(a.button_shape,0,5); a.theme_accent=clamp(a.theme_accent,0,8);
    a.theme_background=clamp(a.theme_background,0,kThemeBackgroundCount-1);
    a.ui_text_percent=90+((clamp(a.ui_text_percent,90,120)-90+5)/10)*10;
    a.poster_size=clamp(a.poster_size,0,2); a.backdrop_dim=clamp(a.backdrop_dim,0,4);
    a.control_style=clamp(a.control_style,0,2); a.description_scroll=clamp(a.description_scroll,0,2);
    return a;
}
inline bool same_appearance(const Appearance &a,const Appearance &b) {
    return a.theme==b.theme && a.colour_variant==b.colour_variant && a.button_shape==b.button_shape && a.theme_accent==b.theme_accent &&
        a.theme_background==b.theme_background && a.ui_text_percent==b.ui_text_percent &&
        a.poster_size==b.poster_size && a.background_artwork==b.background_artwork &&
        a.backdrop_dim==b.backdrop_dim && a.control_style==b.control_style &&
        a.strong_focus==b.strong_focus && a.poster_enlarge==b.poster_enlarge &&
        a.reduced_motion==b.reduced_motion && a.description_scroll==b.description_scroll &&
        a.spoiler_protection==b.spoiler_protection && a.title_as_text==b.title_as_text &&
        a.detail_poster==b.detail_poster;
}
inline Appearance preset_defaults(int id) { Appearance a; a.theme=id; return normalize_appearance(a); }
inline bool appearance_modified(const Appearance &a) {return !same_appearance(a,preset_defaults(a.theme));}
// Limit UTF-8 slot names without splitting a code point. Controls are not stored.
inline std::string theme_slot_name(const std::string &input) {
    std::string out;
    for (size_t i=0;i<input.size() && out.size()<64;) {
        const auto c=static_cast<unsigned char>(input[i]);
        const size_t n=c<0x80?1:(c>=0xc2&&c<=0xdf)?2:(c>=0xe0&&c<=0xef)?3:(c>=0xf0&&c<=0xf4)?4:0;
        if(!n || i+n>input.size()) {++i;continue;}
        bool valid=true;
        for(size_t j=1;j<n;++j) if((static_cast<unsigned char>(input[i+j])&0xc0)!=0x80)valid=false;
        if(n==3 && ((c==0xe0 && (unsigned char)input[i+1]<0xa0) || (c==0xed && (unsigned char)input[i+1]>=0xa0)))valid=false;
        if(n==4 && ((c==0xf0 && (unsigned char)input[i+1]<0x90) || (c==0xf4 && (unsigned char)input[i+1]>=0x90)))valid=false;
        if(!valid){++i;continue;}
        if(c>=32 && c!=127 && out.size()+n<=64)out.append(input,i,n);
        i+=n;
    }
    while(!out.empty() && out.back()==' ')out.pop_back();
    const auto first=out.find_first_not_of(' ');
    return first==std::string::npos?std::string{}:out.substr(first);
}
inline bool same_saved_theme(const SavedAppearance &a,const SavedAppearance &b) {
    return a.valid==b.valid && a.name==b.name && (!a.valid || same_appearance(a.appearance,b.appearance));
}
// No file/network effects. Apply merges only appearance, not a stale Local copy.
class AppearanceEditor {
public:
    bool active=false, saved_valid=false;
    Appearance draft, original, saved;
    std::array<SavedAppearance,kCustomThemes> slots{}, original_slots{};
    int selected_slot=0;
    void begin(const Local &l) {
        draft=original=normalize_appearance(l);slots=l.custom_themes;selected_slot=0;
        if(!slots[0].valid && l.my_theme_valid)slots[0]={true,"My theme",normalize_appearance(l.my_theme)};
        for(auto &slot:slots) {slot.name=theme_slot_name(slot.name);slot.appearance=normalize_appearance(slot.appearance);}
        original_slots=slots;sync_legacy();active=true;
    }
    void sync_legacy(){saved=slots[selected_slot].appearance;saved_valid=slots[selected_slot].valid;}
    void select_slot(int n){selected_slot=std::max(0,std::min(kCustomThemes-1,n));sync_legacy();}
    void reset(){draft=preset_defaults(draft.theme);}
    void restore(){draft=original;}
    void choose_preset(int id){draft=preset_defaults(id);}
    void remember(){auto &s=slots[selected_slot];s.valid=true;s.appearance=normalize_appearance(draft);
        if(s.name.empty())s.name="Custom theme "+std::to_string(selected_slot+1);sync_legacy();}
    void recall(){if(slots[selected_slot].valid)draft=slots[selected_slot].appearance;sync_legacy();}
    void rename(const std::string &name){slots[selected_slot].name=theme_slot_name(name);}
    bool dirty() const {
        if(!same_appearance(draft,original))return true;
        for(size_t i=0;i<slots.size();++i)if(!same_saved_theme(slots[i],original_slots[i]))return true;
        return false;
    }
    std::string status() const {
        for(const auto &s:slots)if(s.valid && same_appearance(draft,s.appearance))return "Saved custom theme: "+s.name;
        return appearance_modified(draft)?"Modified":"Preset defaults";
    }
    Local apply_to(Local current) const {
        static_cast<Appearance&>(current)=normalize_appearance(draft);
        current.custom_themes=slots;
        current.my_theme=slots[0].appearance;current.my_theme_valid=slots[0].valid;
        return current;
    }
    void end(){active=false;}
};
inline bool hide_episode_spoilers(const Appearance &a,const jf::Item &item) {
    return a.spoiler_protection && item.type=="Episode" && !item.played;
}
}
