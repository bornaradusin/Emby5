/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <algorithm>
#include <cmath>

namespace emby5 {
/* A playback chain carries completed episodes and active seconds. Navigation,
 * pause and seek buttons do not silently acknowledge a still-watching prompt. */
class WatchGuard {
public:
    void begin(int completed, double seconds) {
        completed_ = std::max(0, completed);
        seconds_ = std::isfinite(seconds) ? std::max(0.0, seconds) : 0;
        at_ = -1; running_ = false; approved_ = false;
    }
    void sample(double now, bool running) {
        if (!std::isfinite(now)) return;
        if (at_ >= 0 && now >= at_ && running_ && running)
            seconds_ += std::min(1.0, now - at_);
        at_ = now;
        running_ = running;
    }
    bool due(int mode, bool finishing_episode) const {
        return (mode == 1 && finishing_episode && completed_ + 1 >= 3) ||
               (mode == 2 && seconds_ >= 7200.0);
    }
    void approve() { completed_ = 0; seconds_ = 0; approved_ = true; }
    int completed_after(bool automatic_next) const {
        return automatic_next ? (approved_ ? 0 : completed_ + 1) : 0;
    }
    double seconds() const { return seconds_; }
    bool approved() const { return approved_; }
private:
    int completed_ = 0;
    double seconds_ = 0, at_ = -1;
    bool running_ = false, approved_ = false;
};
} // namespace emby5
