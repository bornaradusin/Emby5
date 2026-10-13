/* Read-only release notes and update-available notifications (no installer). */
#pragma once
#include <cstdint>
#include <string>
namespace update_service {
struct Snapshot {
    bool checking=false, available=false;
    std::string latest, message, notes;
    uint64_t revision=0; // lets the idle UI notice completed/failed requests
};
// Called after local settings load and whenever the System switch changes.
// Off suppresses automatic requests AND any notification still in flight.
void set_automatic_enabled(bool enabled);
// Called after sign-in. Repeated calls (including account switches) are coalesced.
void automatic_check();
// User explicitly opened What's New. This never re-enables automatic notifications.
void check();
// Only the main UI thread delivers this text through the existing kernel notifier.
std::string take_notification();
uint64_t generation();
Snapshot snapshot();
}
