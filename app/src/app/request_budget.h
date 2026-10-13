/* Finite catalogue work; a per-read socket timeout alone is not enough. */
#pragma once
#include <algorithm>
#include <chrono>
#include <functional>
namespace emby5 {
class RequestBudget {
    std::chrono::steady_clock::time_point end;
    std::function<bool()> cancelled;
public:
    explicit RequestBudget(int seconds, std::function<bool()> stop = {})
        : end(std::chrono::steady_clock::now()+std::chrono::seconds(std::max(0,seconds))), cancelled(std::move(stop)) {}
    bool expired() const { return (cancelled && cancelled()) || std::chrono::steady_clock::now()>=end; }
    int timeout(int maximum=25) const {
        if(expired())return 0;
        const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(end-std::chrono::steady_clock::now()).count();
        return std::min(maximum,std::max(1,(int)((ms+999)/1000)));
    }
};
}
