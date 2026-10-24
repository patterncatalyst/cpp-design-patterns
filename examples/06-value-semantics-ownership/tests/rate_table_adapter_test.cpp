#include "rate_table_adapter.hpp"

#include <gtest/gtest.h>

namespace warehouse {
namespace {

TEST(RateTableAdapterTest, OpensARecognizedCarrierIntoAUsableHandle) {
    RateTableHandle table = open_rate_table("northwind");

    ASSERT_NE(table, nullptr);
    EXPECT_DOUBLE_EQ(table->per_kg_rate_cents, 285.0);
}

TEST(RateTableAdapterTest, OpensASecondRecognizedCarrierWithItsOwnRate) {
    RateTableHandle table = open_rate_table("cardinal");

    ASSERT_NE(table, nullptr);
    EXPECT_DOUBLE_EQ(table->per_kg_rate_cents, 310.0);
}

TEST(RateTableAdapterTest, ReturnsAnEmptyHandleForAnUnrecognizedCarrier) {
    RateTableHandle table = open_rate_table("unknown-carrier");

    EXPECT_EQ(table, nullptr);
}

TEST(RateTableAdapterTest, HandleOwnsTheTableOutrightAndReleasesItOnScopeExit) {
    // Opens and drops several handles in a row, each one running
    // legacy_close_rate_table() through RateTableDeleter when it goes out
    // of scope, the repeated-allocation shape a leak-detecting build
    // (AddressSanitizer) exercises.
    for (int i = 0; i < 5; ++i) {
        RateTableHandle table = open_rate_table("northwind");
        ASSERT_NE(table, nullptr);
    }
    SUCCEED();
}

}  // namespace
}  // namespace warehouse
