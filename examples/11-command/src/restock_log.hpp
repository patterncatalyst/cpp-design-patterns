#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace warehouse {

// A move-only log of restock events for one SKU. Move-only on purpose:
// copying a receipt would silently fork the audit trail into two, which is
// exactly the kind of bug a command slot holding this by value should make
// impossible rather than merely unlikely.
class RestockReceipt {
public:
    explicit RestockReceipt(std::string sku);

    RestockReceipt(const RestockReceipt&) = delete;
    RestockReceipt& operator=(const RestockReceipt&) = delete;
    RestockReceipt(RestockReceipt&&) = default;
    RestockReceipt& operator=(RestockReceipt&&) = default;

    void append_line(int quantity, const std::string& bin);
    const std::vector<std::string>& lines() const;

private:
    std::string sku_;
    std::vector<std::string> lines_;
};

#if __cplusplus >= 202302L
// C++23: std::move_only_function has no copy requirement, so the callable
// it holds can capture a move-only value directly.
using RestockLogFn = std::move_only_function<void(int, const std::string&)>;
#else
// C++20: std::function requires its target to be copy-constructible, even
// though this target is only ever invoked, never copied.
using RestockLogFn = std::function<void(int, const std::string&)>;
#endif

// Returns a callable that appends a line to receipt each time it runs.
// This is the std::function shortcut this chapter covers alongside the
// full InventoryCommand hierarchy: there is nothing to undo here, just a
// queued side effect, so a bare callable is the whole answer and a class
// implementing execute()/undo() would be ceremony with no second method to
// justify it.
RestockLogFn make_logging_restock(std::unique_ptr<RestockReceipt> receipt);

}  // namespace warehouse
