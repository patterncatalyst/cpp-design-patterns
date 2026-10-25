#include "label_render_thread_pool.hpp"

#include <memory>
#include <utility>

namespace pool24 {

LabelRenderThreadPool::LabelRenderThreadPool(std::size_t worker_count) {
    workers_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this](std::stop_token stop_token) { worker_loop(std::move(stop_token)); });
    }
}

LabelRenderThreadPool::~LabelRenderThreadPool() {
    // Each std::jthread's destructor calls request_stop() and joins. The
    // condition_variable_any::wait() overload used in worker_loop() below
    // registers a stop_callback that wakes every waiting worker the
    // instant any of their stop tokens fires, so no manual notify_all()
    // is needed here. See cpp-concurrency-and-parallelism for exactly how
    // that stop_callback wiring works under the hood.
}

std::future<RenderedLabel> LabelRenderThreadPool::submit(ShippingLabelRequest request) {
    std::packaged_task<RenderedLabel()> packaged(
        [request = std::move(request)] { return render_label(request); });
    std::future<RenderedLabel> future = packaged.get_future();

#if __cplusplus >= 202302L
    Task task(std::move(packaged));
#else
    auto shared_task = std::make_shared<std::packaged_task<RenderedLabel()>>(std::move(packaged));
    Task task = [shared_task] { (*shared_task)(); };
#endif

    {
        std::lock_guard lock(mutex_);
        tasks_.push_back(std::move(task));
    }
    cv_.notify_one();
    return future;
}

void LabelRenderThreadPool::worker_loop(std::stop_token stop_token) {
    while (true) {
        Task task;
        {
            std::unique_lock lock(mutex_);
            const bool got_work = cv_.wait(lock, stop_token, [this] { return !tasks_.empty(); });
            if (!got_work) {
                // Stop was requested and the predicate never became true
                // before that: no more work to drain, exit the loop.
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop_front();
        }
        task();
    }
}

}  // namespace pool24
