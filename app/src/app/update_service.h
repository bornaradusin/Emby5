/* Emby5 in-app GitHub release updater. */
#pragma once
#include <string>
namespace update_service {
struct Snapshot {
    bool checking=false, available=false, downloading=false, staged=false, installing=false;
    std::string latest, message, notes;
};
void check();
void download();
void download_and_install();
void install();
void later();
std::string take_notification();
Snapshot snapshot();
/* App main loop: return true after the independent installer accepts the handoff. */
bool should_exit();
}
