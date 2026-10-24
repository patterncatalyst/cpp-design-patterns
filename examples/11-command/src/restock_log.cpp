#include "restock_log.hpp"

#include <utility>

namespace warehouse {

RestockReceipt::RestockReceipt(std::string sku) : sku_(std::move(sku)) {}

void RestockReceipt::append_line(int quantity, const std::string& bin) {
    lines_.push_back(sku_ + ": +" + std::to_string(quantity) + " into " + bin);
}

const std::vector<std::string>& RestockReceipt::lines() const {
    return lines_;
}

#if __cplusplus >= 202302L
RestockLogFn make_logging_restock(std::unique_ptr<RestockReceipt> receipt) {
    // The lambda captures the unique_ptr directly by move. std::move_only_
    // function places no copy requirement on its target, so there is no
    // workaround to reach for here.
    return [receipt = std::move(receipt)](int quantity, const std::string& bin) mutable {
        receipt->append_line(quantity, bin);
    };
}
#else
RestockLogFn make_logging_restock(std::unique_ptr<RestockReceipt> receipt) {
    // std::function requires its target to be copy-constructible. A lambda
    // that captured the unique_ptr directly would make the closure
    // move-only and fail to compile here, so the receipt is promoted to a
    // shared_ptr first, purely to satisfy that requirement, even though
    // this command slot never actually copies the callable.
    auto shared_receipt = std::shared_ptr<RestockReceipt>(std::move(receipt));
    return [shared_receipt](int quantity, const std::string& bin) { shared_receipt->append_line(quantity, bin); };
}
#endif

}  // namespace warehouse
