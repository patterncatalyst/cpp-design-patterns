#include "scope_guard.hpp"

#include <gtest/gtest.h>

#include <utility>
#include <vector>

namespace warehouse {
namespace {

TEST(ScopeGuardTest, RunsTheActionWhenTheGuardGoesOutOfScope) {
    bool ran = false;
    {
        auto guard = on_scope_exit([&ran] { ran = true; });
        EXPECT_FALSE(ran);
    }
    EXPECT_TRUE(ran);
}

TEST(ScopeGuardTest, DismissCancelsThePendingAction) {
    bool ran = false;
    {
        auto guard = on_scope_exit([&ran] { ran = true; });
        guard.dismiss();
    }
    EXPECT_FALSE(ran);
}

TEST(ScopeGuardTest, RunsActionsInLifoOrderLikeStackDestructors) {
    std::vector<int> executed_order;
    {
        auto guard_a = on_scope_exit([&executed_order] { executed_order.push_back(1); });
        auto guard_b = on_scope_exit([&executed_order] { executed_order.push_back(2); });
        auto guard_c = on_scope_exit([&executed_order] { executed_order.push_back(3); });
        EXPECT_TRUE(executed_order.empty());
    }
    // Local objects destruct in reverse declaration order: guard_c first,
    // then guard_b, then guard_a.
    EXPECT_EQ(executed_order, (std::vector<int>{3, 2, 1}));
}

TEST(ScopeGuardTest, MovingTransfersResponsibilitySoTheActionRunsExactlyOnce) {
    int run_count = 0;
    {
        auto original = on_scope_exit([&run_count] { ++run_count; });
        auto moved = std::move(original);
        (void)moved;
        // original no longer has a pending action; only moved does.
    }
    EXPECT_EQ(run_count, 1);
}

TEST(ScopeGuardTest, ActionRunsDuringExceptionUnwindToo) {
    bool ran = false;

    auto guard_then_throw = [&ran] {
        auto guard = on_scope_exit([&ran] { ran = true; });
        (void)guard;
        throw std::runtime_error("failure after the guard was armed");
    };

    EXPECT_THROW(guard_then_throw(), std::runtime_error);
    EXPECT_TRUE(ran);
}

}  // namespace
}  // namespace warehouse
