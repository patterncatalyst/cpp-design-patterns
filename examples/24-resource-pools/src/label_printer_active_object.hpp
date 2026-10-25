#pragma once

#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <stop_token>
#include <thread>

#include "shipping_label.hpp"

namespace pool24 {

// What a print request resolves to: the rendered label plus the id of the
// thread that actually printed it, which the test suite uses to prove the
// work ran on the active object's own thread, not on whichever thread
// called print_label().
struct PrintResult {
    RenderedLabel label;
    std::thread::id executed_on;
};

// An active object: print_label() only enqueues a request and returns a
// std::future immediately, on the caller's thread. A single dedicated
// worker thread owned by this object pulls requests off its own queue, in
// submission order, and runs them one at a time. That is the whole idea:
// decoupling *when a call is made* from *when it runs*, which a plain
// member function call can never do.
//
// Unlike LabelRenderThreadPool's queue, this one does not need to
// type-erase arbitrary callables: every request has the exact same
// signature, so the queue holds std::packaged_task<PrintResult()>
// directly. That contrast is deliberate: reach for an active object when
// one kind of request needs its own serialized timeline, and a thread
// pool when many different, unrelated jobs share a worker pool.
class LabelPrinterActiveObject {
public:
    LabelPrinterActiveObject();
    ~LabelPrinterActiveObject();

    LabelPrinterActiveObject(const LabelPrinterActiveObject&) = delete;
    LabelPrinterActiveObject& operator=(const LabelPrinterActiveObject&) = delete;

    std::future<PrintResult> print_label(ShippingLabelRequest request);

private:
    void run(std::stop_token stop_token);

    std::mutex mutex_;
    std::condition_variable_any cv_;
    std::deque<std::packaged_task<PrintResult()>> requests_;
    std::jthread worker_;
};

}  // namespace pool24
