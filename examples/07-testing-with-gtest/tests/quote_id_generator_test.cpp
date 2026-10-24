#include "quote_id_generator.hpp"

#include <gtest/gtest.h>

#include <unordered_set>

namespace freight {
namespace {

// A typed test suite: the same two checks, run once per concrete generator
// type, rather than copy-pasted for SequentialQuoteIdGenerator and again
// for PrefixedQuoteIdGenerator. Both types satisfy the QuoteIdGenerator
// contract described in quote_id_generator.hpp despite numbering ids in
// completely different ways underneath; this suite is what proves that
// claim instead of just asserting it in a comment.
template <typename GeneratorType>
class QuoteIdGeneratorContractTest : public ::testing::Test {
protected:
    GeneratorType generator_;
};

using QuoteIdGeneratorTypes = ::testing::Types<SequentialQuoteIdGenerator, PrefixedQuoteIdGenerator>;
TYPED_TEST_SUITE(QuoteIdGeneratorContractTest, QuoteIdGeneratorTypes);

TYPED_TEST(QuoteIdGeneratorContractTest, NeverReturnsAnEmptyId) {
    EXPECT_FALSE(this->generator_.next_id().empty());
}

TYPED_TEST(QuoteIdGeneratorContractTest, NeverRepeatsAcrossOneHundredCalls) {
    std::unordered_set<std::string> seen;
    for (int call = 0; call < 100; ++call) {
        const bool first_time_seen = seen.insert(this->generator_.next_id()).second;
        ASSERT_TRUE(first_time_seen) << "a duplicate id was produced on call " << call;
    }
}

}  // namespace
}  // namespace freight
