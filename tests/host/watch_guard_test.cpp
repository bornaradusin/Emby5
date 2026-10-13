#include "app/watch_guard.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main() {
    emby5::WatchGuard w;
    for (int completed = 0; completed < 3; ++completed) {
        w.begin(completed, 0);
        assert(w.due(1, true) == (completed == 2));
        assert(!w.due(1, false));
        assert(!w.due(0, true));
    }
    w.approve();
    assert(!w.due(1, true));
    assert(w.completed_after(true) == 0);
    w.begin(2, 0);
    assert(w.completed_after(false) == 0); // manual next starts a new chain
    w.begin(0, 0);
    w.sample(0, true);
    for (int i=1; i<=3600; ++i) w.sample(i, true);
    assert(w.seconds() == 3600 && !w.due(2, false));
    w.sample(3601, false);
    w.sample(9999, false); // paused, buffering or a still-watching prompt
    w.sample(10000, true);
    assert(w.seconds() == 3600);
    for (int i=10001; i<=13600; ++i) w.sample(i, true);
    assert(w.seconds() == 7200 && w.due(2, false));
    w.approve();
    assert(w.seconds() == 0 && !w.due(2, false));
    w.begin(-3, NAN); assert(w.seconds() == 0);
    w.sample(5, true); w.sample(4, true); assert(w.seconds() == 0);
    w.sample(10004, true); assert(w.seconds() == 1); // no clock-jump inflation
    std::cout << "PASS: episode 1/2/3, acknowledgement, manual next, pause/buffer time, 2-hour threshold, bad clock\n";
}
