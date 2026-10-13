/* Fair, cancellable catalogue admission. SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <unistd.h>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace iptv_catalogue {
inline std::atomic<unsigned> &playback_users(){static std::atomic<unsigned> count{0};return count;}
inline bool playback_active(){return playback_users().load()!=0;}
// Catalogue metadata is optional; it must not compete with the stream selected
// by the viewer. Its verified partial results are retained for the next refresh.
struct PlaybackPriority {
    PlaybackPriority(){playback_users().fetch_add(1);}
    ~PlaybackPriority(){playback_users().fetch_sub(1);}
    PlaybackPriority(const PlaybackPriority&)=delete;
    PlaybackPriority& operator=(const PlaybackPriority&)=delete;
};
// Bulk Live/Movie/Series API requests share a single turn per provider. The
// turn is released between categories; an entire scan never monopolises it.
// Playback and user-data API operations do not use this queue.
class RequestTurn {
    struct Queue {std::mutex mutex;std::deque<const void*> waiting;bool busy=false;};
    std::shared_ptr<Queue> queue_;
    bool owned_=false;
    static std::shared_ptr<Queue> queue(const std::string &key) {
        static std::mutex mutex;
        static std::map<std::string,std::weak_ptr<Queue>> queues;
        std::lock_guard<std::mutex> lock(mutex);
        for(auto it=queues.begin();it!=queues.end();)if(it->second.expired())it=queues.erase(it);else ++it;
        auto &slot=queues[key];auto q=slot.lock();if(!q){q=std::make_shared<Queue>();slot=q;}return q;
    }
public:
    RequestTurn(const std::string &key,const std::function<bool()> &cancel):queue_(queue(key)) {
        std::unique_lock<std::mutex> lock(queue_->mutex);
        queue_->waiting.push_back(this);
        for(;;) {
            // Cancellation may take a catalogue mutex. Do not invoke it while
            // holding the admission mutex (no cross-subsystem lock inversion).
            lock.unlock();const bool stopped=cancel&&cancel();lock.lock();
            if(stopped){queue_->waiting.erase(std::find(queue_->waiting.begin(),queue_->waiting.end(),this));return;}
            if(!queue_->busy&&queue_->waiting.front()==this) {
                queue_->waiting.pop_front();queue_->busy=true;owned_=true;return;
            }
            lock.unlock();usleep(10000);lock.lock();
        }
    }
    ~RequestTurn(){if(owned_){std::lock_guard<std::mutex> lock(queue_->mutex);queue_->busy=false;}}
    explicit operator bool() const{return owned_;}
    RequestTurn(const RequestTurn&)=delete;
    RequestTurn& operator=(const RequestTurn&)=delete;
};
}
