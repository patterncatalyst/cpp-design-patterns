#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <future>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

#if __cplusplus >= 202302L
#include <functional>  // std::move_only_function
#endif

#include "shipping_label.hpp"

namespace pool24 {

// A fixed-size pool of worker threads draining a shared task queue, the
// shape most people mean by "thread pool." Submitting work returns a
// std::future immediately; the caller never blocks on submit(), only (if
// it chooses to) on the future it gets back. This example renders
// shipping labels, but the queue itself is generic: submit() accepts any
// callable, the way a production thread pool would.
//
// The deep mechanics of std::jthread, std::stop_token, and the
// std::condition_variable_any stop_token overload used below are covered
// in full in the sibling book cpp-concurrency-and-parallelism; this
// chapter only needs the pattern shape, worker threads pulling from a
// shared queue until told to stop.
class LabelRenderThreadPool {
public:
    explicit LabelRenderThreadPool(std::size_t worker_count);
    ~LabelRenderThreadPool();

    LabelRenderThreadPool(const LabelRenderThreadPool&) = delete;
    LabelRenderThreadPool& operator=(const LabelRenderThreadPool&) = delete;

    std::future<RenderedLabel> submit(ShippingLabelRequest request);

    std::size_t worker_count() const { return workers_.size(); }

private:
#if __cplusplus >= 202302L
    // C++23: std::move_only_function stores the move-only
    // std::packaged_task directly, no extra indirection required.
    using Task = std::move_only_function<void()>;
#else
    // C++20: std::function requires its target to be copy-constructible.
    // std::packaged_task is move-only, so submit() below wraps it in a
    // shared_ptr (which IS copyable) before it can live in a
    // std::function. That wrapping costs an extra heap allocation and a
    // reference count per submitted task that the C++23 path does not
    // pay.
    using Task = std::function<void()>;
#endif

    void worker_loop(std::stop_token stop_token);

    std::mutex mutex_;
    std::condition_variable_any cv_;
    std::deque<Task> tasks_;
    std::vector<std::jthread> workers_;
};

}  // namespace pool24
