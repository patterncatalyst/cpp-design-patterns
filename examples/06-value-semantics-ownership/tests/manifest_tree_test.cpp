#include "manifest_tree.hpp"

#include <gtest/gtest.h>

namespace warehouse {
namespace {

TEST(ManifestTreeTest, ChildCanLockItsParentWhileTheParentIsAlive) {
    auto root = make_root_manifest("M-1000");
    auto child = root->add_child("M-1000-A");

    auto locked_parent = child->parent().lock();

    ASSERT_NE(locked_parent, nullptr);
    EXPECT_EQ(locked_parent->manifest_id(), "M-1000");
}

TEST(ManifestTreeTest, RootOwnsItsChildrenByValueOfSharedPtr) {
    auto root = make_root_manifest("M-2000");
    root->add_child("M-2000-A");
    root->add_child("M-2000-B");

    ASSERT_EQ(root->children().size(), 2u);
    EXPECT_EQ(root->children()[0]->manifest_id(), "M-2000-A");
    EXPECT_EQ(root->children()[1]->manifest_id(), "M-2000-B");
}

TEST(ManifestTreeTest, WeakPtrExpiresWhenTheOwningRootIsDestroyed) {
    auto root = make_root_manifest("M-3000");
    auto child = root->add_child("M-3000-A");

    EXPECT_FALSE(child->parent().expired());

    root.reset();

    // The child outlives the root here because the test still holds a
    // shared_ptr to it directly. If parent() were a shared_ptr instead of
    // a weak_ptr, root.reset() would not destroy the root at all: the
    // child (alive via the test's own shared_ptr) would still hold a
    // strong reference back to it, and neither object would ever reach a
    // reference count of zero.
    EXPECT_TRUE(child->parent().expired());
    EXPECT_EQ(child->parent().lock(), nullptr);
}

TEST(ManifestTreeTest, NestedGrandchildAlsoLosesItsLockableAncestorChain) {
    auto root = make_root_manifest("M-4000");
    auto mid = root->add_child("M-4000-A");
    auto leaf = mid->add_child("M-4000-A-1");

    root.reset();
    mid.reset();

    EXPECT_TRUE(leaf->parent().expired());
}

}  // namespace
}  // namespace warehouse
