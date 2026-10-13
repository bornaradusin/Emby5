/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/io_worker.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
int main() {
    auto stack=std::make_shared<std::promise<size_t>>();auto result=stack->get_future();
    assert(emby5::spawn_io([stack] {
        size_t bytes=emby5::io_worker::stack_bytes;
#ifdef __linux__
        pthread_attr_t attr; assert(pthread_getattr_np(pthread_self(),&attr)==0);
        assert(pthread_attr_getstacksize(&attr,&bytes)==0);pthread_attr_destroy(&attr);
#endif
        stack->set_value(bytes);
    }));
    assert(result.get()>=1024*1024);
    std::atomic<int> active{0},peak{0},done{0};
    std::vector<std::function<void()>> jobs;
    for(int i=0;i<60;++i)jobs.push_back([&] {
        int n=++active,old=peak.load();while(n>old&&!peak.compare_exchange_weak(old,n)){}
        std::this_thread::sleep_for(std::chrono::milliseconds(2));++done;--active;
    });
    emby5::run_io_jobs(jobs);assert(done==60 && active==0 && peak<=3);
    std::promise<void> failed;auto caught=failed.get_future();
    assert(emby5::spawn_io([] {throw std::runtime_error("injected failure");},[&]{failed.set_value();}));caught.get();
    jobs={[]{throw std::runtime_error("injected failure");},[&]{++done;}};
    emby5::run_io_jobs(jobs);assert(done==61);
    std::cout<<"PASS: explicit >=1 MiB IO stack, <=3 parallel jobs, joins, failure callbacks and exception containment\n";
}
