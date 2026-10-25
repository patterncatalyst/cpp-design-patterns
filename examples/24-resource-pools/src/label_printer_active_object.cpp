#include "label_printer_active_object.hpp"

#include <utility>

namespace pool24 {

LabelPrinterActiveObject::LabelPrinterActiveObject()
    : worker_([this](std::stop_token stop_token) { run(std::move(stop_token)); }) {}

LabelPrinterActiveObject::~LabelPrinterActiveObject() {
    // std::jthread's destructor requests a stop and joins; the
    // condition_variable_any::wait() overload in run() wakes on that stop
    // request the same way LabelRenderThreadPool's workers do.
}

std::future<PrintResult> LabelPrinterActiveObject::print_label(ShippingLabelRequest request) {
    std::packaged_task<PrintResult()> task([request = std::move(request)] {
        return PrintResult{.label = render_label(request), .executed_on = std::this_thread::get_id()};
    });
    std::future<PrintResult> future = task.get_future();

    {
        std::lock_guard lock(mutex_);
        requests_.push_back(std::move(task));
    }
    cv_.notify_one();
    return future;
}

void LabelPrinterActiveObject::run(std::stop_token stop_token) {
    while (true) {
        std::packaged_task<PrintResult()> task;
        {
            std::unique_lock lock(mutex_);
            const bool got_work = cv_.wait(lock, stop_token, [this] { return !requests_.empty(); });
            if (!got_work) {
                return;
            }
            task = std::move(requests_.front());
            requests_.pop_front();
        }
        task();
    }
}

}  // namespace pool24
