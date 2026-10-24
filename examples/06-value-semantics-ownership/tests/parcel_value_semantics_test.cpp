#include "parcel.hpp"

#include <gtest/gtest.h>

#include <utility>

namespace warehouse {
namespace {

TEST(ParcelValueSemanticsTest, CopyIsIndependentOfTheOriginal) {
    Parcel original{"TRK-1001", 4.2, "Fragile"};
    Parcel copy = original;

    copy.weight_kg = 9.9;
    copy.label = "Re-weighed";

    EXPECT_DOUBLE_EQ(original.weight_kg, 4.2);
    EXPECT_EQ(original.label, "Fragile");
    EXPECT_DOUBLE_EQ(copy.weight_kg, 9.9);
    EXPECT_EQ(copy.label, "Re-weighed");
}

TEST(ParcelValueSemanticsTest, CopyAssignmentAlsoDeepCopies) {
    Parcel original{"TRK-1002", 2.0, "Standard"};
    Parcel other{"TRK-9999", 0.0, ""};

    other = original;
    other.tracking_id = "TRK-0000";

    EXPECT_EQ(original.tracking_id, "TRK-1002");
    EXPECT_EQ(other.tracking_id, "TRK-0000");
}

TEST(ParcelValueSemanticsTest, MoveLeavesAValidButUnspecifiedSource) {
    Parcel original{"TRK-1003", 3.3, "Perishable"};
    Parcel moved_to = std::move(original);

    EXPECT_EQ(moved_to.tracking_id, "TRK-1003");
    EXPECT_DOUBLE_EQ(moved_to.weight_kg, 3.3);
}

}  // namespace
}  // namespace warehouse
