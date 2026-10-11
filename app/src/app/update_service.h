/* Read-only release notes and update-available notifications (no installer). */
#pragma once
#include <string>
namespace update_service {
struct Snapshot {
    bool checking=false, available=false;
    std::string latest, message, notes;
};
void check();
std::string take_notification();
Snapshot snapshot();
}
