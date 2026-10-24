#include "restock_log.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace warehouse {
namespace {

TEST(RestockLogTest, InvokingAppendsALogLine) {
    auto receipt = std::make_unique<RestockReceipt>("GASKET-7");
    const RestockReceipt* receipt_view = receipt.get();
    RestockLogFn log_restock = make_logging_restock(std::move(receipt));

    log_restock(30, "B2");

    ASSERT_EQ(receipt_view->lines().size(), 1u);
    EXPECT_EQ(receipt_view->lines().at(0), "GASKET-7: +30 into B2");
}

TEST(RestockLogTest, MultipleInvocationsAppendInOrder) {
    auto receipt = std::make_unique<RestockReceipt>("GASKET-7");
    const RestockReceipt* receipt_view = receipt.get();
    RestockLogFn log_restock = make_logging_restock(std::move(receipt));

    log_restock(30, "B2");
    log_restock(5, "B2");

    ASSERT_EQ(receipt_view->lines().size(), 2u);
    EXPECT_EQ(receipt_view->lines().at(0), "GASKET-7: +30 into B2");
    EXPECT_EQ(receipt_view->lines().at(1), "GASKET-7: +5 into B2");
}

}  // namespace
}  // namespace warehouse
