#include "rate_quote_ledger.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace shipping {

// Every bookkeeping detail lives here, invisible to anything that only
// includes rate_quote_ledger.hpp: the storage shape (a vector of pairs
// today), the running-total loop, and the exact summary format can all
// change without forcing a single recompile outside this one .cpp file.
class RateQuoteLedger::Impl {
public:
    void record(std::string label, Cents amount_cents) { entries_.emplace_back(std::move(label), amount_cents); }

    int entry_count() const { return static_cast<int>(entries_.size()); }

    Cents total_cents() const {
        Cents total = 0;
        for (const auto& entry : entries_) {
            total += entry.second;
        }
        return total;
    }

    std::string summary() const {
        std::ostringstream out;
        out << entries_.size() << " quote(s), total " << total_cents() << " cents:";
        for (const auto& entry : entries_) {
            out << "\n  " << entry.first << ": " << entry.second << " cents";
        }
        return out.str();
    }

private:
    std::vector<std::pair<std::string, Cents>> entries_;
};

RateQuoteLedger::RateQuoteLedger() : impl_(std::make_unique<Impl>()) {}

RateQuoteLedger::~RateQuoteLedger() = default;

RateQuoteLedger::RateQuoteLedger(const RateQuoteLedger& other) : impl_(std::make_unique<Impl>(*other.impl_)) {}

RateQuoteLedger& RateQuoteLedger::operator=(const RateQuoteLedger& other) {
    if (this != &other) {
        impl_ = std::make_unique<Impl>(*other.impl_);
    }
    return *this;
}

RateQuoteLedger::RateQuoteLedger(RateQuoteLedger&& other) noexcept = default;

RateQuoteLedger& RateQuoteLedger::operator=(RateQuoteLedger&& other) noexcept = default;

void RateQuoteLedger::record(std::string label, Cents amount_cents) {
    if (amount_cents < 0) {
        throw std::invalid_argument("quote amount_cents cannot be negative");
    }
    impl_->record(std::move(label), amount_cents);
}

int RateQuoteLedger::entry_count() const {
    return impl_->entry_count();
}

Cents RateQuoteLedger::total_cents() const {
    return impl_->total_cents();
}

std::string RateQuoteLedger::summary() const {
    return impl_->summary();
}

#if __cplusplus >= 202302L
std::expected<void, std::string> RateQuoteLedger::try_record(std::string label, Cents amount_cents) {
    if (amount_cents < 0) {
        return std::unexpected<std::string>("quote amount_cents cannot be negative");
    }
    impl_->record(std::move(label), amount_cents);
    return {};
}
#endif

}  // namespace shipping
