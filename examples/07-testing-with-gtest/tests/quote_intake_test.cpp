#include "quote_intake.hpp"

#include <gtest/gtest.h>

#include "mock_quote_ledger.hpp"
#include "quote_id_generator.hpp"

namespace freight {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Return;

// QuoteIntakeTest shares a fixture across every test below: a mock ledger,
// a real SequentialQuoteIdGenerator, the intake under test wired to both,
// and one baseline valid request every test can reuse.
//
// SetUp/TearDown earn their place here for a reason beyond convenience:
// SequentialQuoteIdGenerator's counter is process-wide (quote_id_generator.hpp
// explains why), not reset automatically just because this fixture
// constructs a fresh generator member for every test. Without an explicit
// reset, the quote_id a test observes would depend on how many ids every
// earlier test in this binary already consumed. TearDown resets the
// counter again, not only SetUp, because a fatal ASSERT_* failure midway
// through a test body jumps straight to TearDown without running the rest
// of that test's code; cleanup that only happens at the bottom of the test
// body would silently skip on exactly the runs where it matters most.
class QuoteIntakeTest : public ::testing::Test {
protected:
    void SetUp() override { SequentialQuoteIdGenerator::reset_sequence_for_test(1); }
    void TearDown() override { SequentialQuoteIdGenerator::reset_sequence_for_test(1); }

    MockQuoteLedger ledger_;
    SequentialQuoteIdGenerator id_generator_;
    QuoteIntake intake_{ledger_, id_generator_};
    const ShipmentRequest valid_request_{3.5, ServiceZone::Regional, 12'000};
};

TEST_F(QuoteIntakeTest, AcceptsAndRecordsAValidShipment) {
    EXPECT_CALL(ledger_, record(_)).WillOnce(Return(true));

    const auto result = intake_.submit(valid_request_);

    // ASSERT_TRUE is fatal: if submit() returned std::nullopt, dereferencing
    // result below would be undefined behavior, so the test must stop here
    // rather than continue and crash the whole test binary.
    ASSERT_TRUE(result.has_value()) << "a valid shipment must produce a quote, not std::nullopt";
    // EXPECT_EQ is not fatal: both checks below run and both get reported
    // even if one of them fails, which matters when a regression breaks
    // the price but leaves the id alone, or the other way around.
    EXPECT_EQ(result->amount_cents, 1225) << "pricing changed; check quote_cost()'s weight and zone math";
    EXPECT_EQ(result->quote_id, "Q-0001");
}

TEST_F(QuoteIntakeTest, RejectsAnInvalidShipmentWithoutTouchingTheLedger) {
    // Times(0) makes the interaction itself the assertion: a request that
    // fails validation must never reach the ledger at all, not reach it
    // and get declined.
    EXPECT_CALL(ledger_, record(_)).Times(0);
    const ShipmentRequest malformed{-1.0, ServiceZone::Local, 500};

    const auto result = intake_.submit(malformed);

    EXPECT_FALSE(result.has_value());
}

TEST_F(QuoteIntakeTest, ReturnsNulloptWhenTheLedgerDeclinesToRecord) {
    EXPECT_CALL(ledger_, record(_)).WillOnce(Return(false));

    const auto result = intake_.submit(valid_request_);

    EXPECT_FALSE(result.has_value()) << "submit() must defer to the ledger's answer, not just its own validation";
}

TEST_F(QuoteIntakeTest, PassesTheComputedAmountToTheLedgerBeforeRecording) {
    // A matcher on the argument record() actually receives, not just on
    // whether it was called: this is the check that would catch a bug
    // where QuoteIntake records the wrong price while still returning the
    // right one to its own caller.
    EXPECT_CALL(ledger_, record(Field(&Quote::amount_cents, Eq(1225)))).WillOnce(Return(true));

    const auto result = intake_.submit(valid_request_);

    ASSERT_TRUE(result.has_value());
}

}  // namespace
}  // namespace freight
