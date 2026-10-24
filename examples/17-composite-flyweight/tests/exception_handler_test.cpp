#include "exception_handler.hpp"

#include <gtest/gtest.h>

namespace palletizing {
namespace {

TEST(NullShipmentExceptionHandlerTest, IsSafelyCallableAndDoesNothing) {
    const ShipmentExceptionHandler& handler = null_shipment_exception_handler();

    // The call below is the branch-free path Null Object exists to enable:
    // no "if the handler exists" check, just a call. EXPECT_NO_THROW proves
    // it is safe to call unconditionally; there is nothing else to observe
    // because a no-op has no side effect to assert on.
    EXPECT_NO_THROW(handler.on_overweight("Pallet PAL-1", 500));
}

TEST(NullShipmentExceptionHandlerTest, EveryCallReturnsTheSameSharedInstance) {
    const ShipmentExceptionHandler& first = null_shipment_exception_handler();
    const ShipmentExceptionHandler& second = null_shipment_exception_handler();

    EXPECT_EQ(&first, &second);
}

TEST(LoggingShipmentExceptionHandlerTest, RecordsAnOverweightEvent) {
    LoggingShipmentExceptionHandler logger;

    logger.on_overweight("Carton CTN-A", 1500);

    ASSERT_EQ(logger.events().size(), 1);
    EXPECT_EQ(logger.events().front(), "Carton CTN-A is 1500g over its weight limit");
}

TEST(LoggingShipmentExceptionHandlerTest, RecordsEveryCallInOrder) {
    LoggingShipmentExceptionHandler logger;

    logger.on_overweight("Carton CTN-A", 100);
    logger.on_overweight("Carton CTN-B", 200);

    ASSERT_EQ(logger.events().size(), 2);
    EXPECT_EQ(logger.events()[0], "Carton CTN-A is 100g over its weight limit");
    EXPECT_EQ(logger.events()[1], "Carton CTN-B is 200g over its weight limit");
}

}  // namespace
}  // namespace palletizing
