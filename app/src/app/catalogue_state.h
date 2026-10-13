/* Emby5 catalogue refresh state. SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>
namespace iptv_catalogue {
struct Category { std::string id, name; };
// completed contains ONLY categories replaced by this attempt. failed is a
// resumable plan; an empty ID means the whole-list request must be retried.
struct Scan {
    std::vector<Category> categories;
    std::unordered_set<std::string> completed;
    std::vector<std::string> failed;
    bool list_ok=false, whole=false, complete=false, changed=false, incremental=false;
    size_t done=0, total=0;
    std::string error;
};
struct State {
    bool configured=false, loading=false, cached=false, partial=false;
    size_t done=0, total=0, failed=0;
    std::string error;
};
inline State result_state(const Scan &scan, bool retained) {
    State s; s.configured=true; s.cached=retained; s.partial=!scan.complete&&scan.changed;
    s.done=scan.done; s.total=scan.total; s.failed=scan.failed.size(); s.error=scan.error;
    return s;
}
// Compact, credential-free status for the Settings rows and catalogue screens.
inline std::string describe(const State &s, size_t count, const char *unit) {
    if(!s.configured) return "Not configured";
    std::string text=std::to_string(count)+" "+unit;
    if(s.loading) {
        text+=count?" - refreshing":" - loading";
        if(s.total)text+=" ("+std::to_string(s.done)+"/"+std::to_string(s.total)+")";
    } else if(!s.error.empty()) {
        text+=s.partial?" - partial":((s.cached&&count)?" - cached":" - failed");
        if(s.failed)text+="; "+std::to_string(s.failed)+" failed/skipped";
        text+=" ("+s.error+"); X to retry";
    } else text+=s.cached?" - cached":" - ready";
    return text;
}
}
