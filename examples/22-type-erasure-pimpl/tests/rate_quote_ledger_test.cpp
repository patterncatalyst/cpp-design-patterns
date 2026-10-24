#include "rate_quote_ledger.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <utility>

namespace shipping {
namespace {

TEST(RateQuoteLedgerTest, RecordsEntriesAndAccumulatesTheTotal) {
    RateQuoteLedger ledger;
    ledger.record("flat-rate quote", 999);
    ledger.record("weight-based quote", 1025);

    EXPECT_EQ(ledger.entry_count(), 2);
    EXPECT_EQ(ledger.total_cents(), 2024);
}

TEST(RateQuoteLedgerTest, RejectsANegativeAmount) {
    RateQuoteLedger ledger;

    EXPECT_THROW(ledger.record("refund", -100), std::invalid_argument);
}

TEST(RateQuoteLedgerTest, SummaryMentionsEveryRecordedLabel) {
    RateQuoteLedger ledger;
    ledger.record("flat-rate quote", 999);

    const std::string summary = ledger.summary();

    EXPECT_NE(summary.find("flat-rate quote"), std::string::npos);
    EXPECT_NE(summary.find("999"), std::string::npos);
}

TEST(RateQuoteLedgerTest, CopyIsADeepCloneIndependentOfTheOriginal) {
    RateQuoteLedger original;
    original.record("flat-rate quote", 999);

    RateQuoteLedger copy = original;
    copy.record("weight-based quote", 1025);

    EXPECT_EQ(original.entry_count(), 1);
    EXPECT_EQ(original.total_cents(), 999);
    EXPECT_EQ(copy.entry_count(), 2);
    EXPECT_EQ(copy.total_cents(), 2024);
}

TEST(RateQuoteLedgerTest, CopyAssignmentReplacesTheTargetWithAnIndependentClone) {
    RateQuoteLedger original;
    original.record("flat-rate quote", 999);
    RateQuoteLedger other;
    other.record("some other quote", 1);

    other = original;
    other.record("weight-based quote", 1025);

    EXPECT_EQ(original.entry_count(), 1);
    EXPECT_EQ(other.entry_count(), 2);
}

TEST(RateQuoteLedgerTest, MoveTransfersTheLedgerContents) {
    RateQuoteLedger original;
    original.record("flat-rate quote", 999);

    RateQuoteLedger moved = std::move(original);

    EXPECT_EQ(moved.entry_count(), 1);
    EXPECT_EQ(moved.total_cents(), 999);
}

TEST(RateQuoteLedgerTest, HeaderHidesTheImplBehindASinglePointer) {
    // RateQuoteLedger's only data member is a std::unique_ptr<Impl>, and
    // the header never defines Impl, so this object's size is exactly one
    // pointer regardless of how much bookkeeping Impl eventually grows to
    // hold.
    EXPECT_EQ(sizeof(RateQuoteLedger), sizeof(void*));
}

#if __cplusplus >= 202302L
TEST(RateQuoteLedgerTest, TryRecordRejectsANegativeAmountUnderCpp23) {
    RateQuoteLedger ledger;

    const auto result = ledger.try_record("refund", -100);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "quote amount_cents cannot be negative");
    EXPECT_EQ(ledger.entry_count(), 0);
}

TEST(RateQuoteLedgerTest, TryRecordAcceptsAValidAmountUnderCpp23) {
    RateQuoteLedger ledger;

    const auto result = ledger.try_record("flat-rate quote", 999);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(ledger.entry_count(), 1);
    EXPECT_EQ(ledger.total_cents(), 999);
}
#endif

}  // namespace
}  // namespace shipping
