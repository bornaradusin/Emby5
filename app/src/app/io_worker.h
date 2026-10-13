/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <array>
#include <functional>
#include <memory>
#include <pthread.h>
#include <vector>

namespace emby5 {
namespace io_worker {
// AVIO and JSON calls must not depend on a platform's small default thread stack.
constexpr size_t stack_bytes = 1024u * 1024u;
struct Job { std::function<void()> run, failed; };
inline void invoke(Job &j) noexcept {
    try { j.run(); }
    catch (...) { if (j.failed) { try { j.failed(); } catch (...) {} } }
}
inline void *entry(void *opaque) noexcept {
    std::unique_ptr<Job> j(static_cast<Job *>(opaque));
    invoke(*j);
    return nullptr;
}
inline bool start(pthread_t *thread, const std::function<void()> &run,
                  const std::function<void()> &failed = {}, bool detached = true) noexcept {
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) return false;
    int rc = pthread_attr_setstacksize(&attr, stack_bytes);

    Job *job = nullptr;
    if (!rc) { try { job = new Job{run, failed}; } catch (...) { rc = -1; } }
    if (!rc) rc = pthread_create(thread, &attr, entry, job);
    pthread_attr_destroy(&attr);
    if (rc) delete job;
    else if (detached && pthread_detach(*thread) != 0) pthread_join(*thread, nullptr);
    return rc == 0;
}
}
inline bool spawn_io(std::function<void()> run, std::function<void()> failed = {}) {
    pthread_t thread;
    return io_worker::start(&thread, std::move(run), std::move(failed));
}
// Called only on an IO worker. At most three additional workers at once; a refused
// launch executes on the caller, and started workers are joined before returning.
inline void run_io_jobs(const std::vector<std::function<void()>> &jobs) {
    for (size_t base = 0; base < jobs.size(); base += 3) {
        std::array<pthread_t, 3> threads{};
        size_t used = 0;
        for (size_t i = base; i < jobs.size() && i < base + 3; ++i) {
            if (io_worker::start(&threads[used], jobs[i], {}, false)) ++used;
            else { try { jobs[i](); } catch (...) {} }
        }
        for (size_t i = 0; i < used; ++i) pthread_join(threads[i], nullptr);
    }
}
}
