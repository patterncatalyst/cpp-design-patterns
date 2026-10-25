#pragma once

#include <chrono>

namespace refactor29 {

// The seam this example's second stage introduces: Extract Interface
// pulled "what day is it" out of a direct std::chrono::system_clock::now()
// call and gave it a name a test can substitute.
class Clock {
public:
    virtual ~Clock() = default;
    virtual std::chrono::year_month_day today() const = 0;
};

// The production implementation. Behaviorally identical to the inline
// call the legacy estimator makes; the only difference is that it now
// sits behind Clock instead of being written directly into estimate().
class SystemClock final : public Clock {
public:
    std::chrono::year_month_day today() const override {
        const auto now = std::chrono::system_clock::now();
        return std::chrono::year_month_day{
            std::chrono::floor<std::chrono::days>(now)};
    }
};

// The test double the seam exists to make possible. A characterization
// test cannot pin an answer that changes with the calendar; FixedClock
// holds "today" still so the estimators become fully deterministic.
class FixedClock final : public Clock {
public:
    explicit FixedClock(std::chrono::year_month_day date) : date_(date) {}

    std::chrono::year_month_day today() const override { return date_; }

private:
    std::chrono::year_month_day date_;
};

}  // namespace refactor29
