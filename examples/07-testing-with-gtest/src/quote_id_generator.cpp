#include "quote_id_generator.hpp"

#include <cstdio>
#include <utility>

namespace freight {

int SequentialQuoteIdGenerator::next_value_ = 1;

std::string SequentialQuoteIdGenerator::next_id() {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "Q-%04d", next_value_);
    ++next_value_;
    return buffer;
}

void SequentialQuoteIdGenerator::reset_sequence_for_test(int next_value) {
    next_value_ = next_value;
}

PrefixedQuoteIdGenerator::PrefixedQuoteIdGenerator(std::string prefix) : prefix_(std::move(prefix)) {}

std::string PrefixedQuoteIdGenerator::next_id() {
    return prefix_ + "-" + std::to_string(next_value_++);
}

}  // namespace freight
