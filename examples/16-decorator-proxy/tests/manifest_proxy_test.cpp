#include "manifest_proxy.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

namespace shipping {
namespace {

TEST(LazyManifestStoreProxyTest, DoesNotConstructTheRealSubjectUntilFirstLookup) {
    LazyManifestStoreProxy proxy;

    EXPECT_FALSE(proxy.is_loaded());

    const std::string manifest = proxy.lookup("TRK-1001");

    EXPECT_TRUE(proxy.is_loaded());
    EXPECT_EQ(manifest, "FASTSHIP-MANIFEST acct=acct-fastship-77 dest=CA");
}

TEST(LazyManifestStoreProxyTest, ForwardsSubsequentLookupsToTheSameRealSubject) {
    LazyManifestStoreProxy proxy;
    proxy.lookup("TRK-1001");
    ASSERT_TRUE(proxy.is_loaded());

    EXPECT_EQ(proxy.lookup("TRK-2002"), "SF-MANIFEST account:acct-summit-42 to:CA");
}

TEST(AccessControlledManifestProxyTest, DeniesLookupBelowRequiredClearance) {
    AccessControlledManifestProxy proxy(std::make_unique<ArchivedManifestStore>(), AccessLevel::Guest);

    EXPECT_THROW(proxy.lookup("TRK-1001"), std::runtime_error);
}

TEST(AccessControlledManifestProxyTest, ForwardsLookupAtOrAboveRequiredClearance) {
    AccessControlledManifestProxy proxy(std::make_unique<ArchivedManifestStore>(), AccessLevel::Warehouse);

    EXPECT_EQ(proxy.lookup("TRK-1001"), "FASTSHIP-MANIFEST acct=acct-fastship-77 dest=CA");
}

TEST(AccessControlledManifestProxyTest, ComposesWithTheLazyProxyUnderneathIt) {
    AccessControlledManifestProxy proxy(std::make_unique<LazyManifestStoreProxy>(), AccessLevel::Admin);

    EXPECT_EQ(proxy.lookup("TRK-2002"), "SF-MANIFEST account:acct-summit-42 to:CA");
}

#if __cplusplus >= 202302L
TEST(AccessControlledManifestProxyTest, TryLookupReturnsTypedErrorBelowRequiredClearance) {
    AccessControlledManifestProxy proxy(std::make_unique<ArchivedManifestStore>(), AccessLevel::Guest);

    const auto result = proxy.try_lookup("TRK-1001");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "access denied: manifest lookup requires warehouse clearance");
}

TEST(AccessControlledManifestProxyTest, TryLookupReturnsTheManifestAtOrAboveRequiredClearance) {
    AccessControlledManifestProxy proxy(std::make_unique<ArchivedManifestStore>(), AccessLevel::Admin);

    const auto result = proxy.try_lookup("TRK-2002");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "SF-MANIFEST account:acct-summit-42 to:CA");
}
#endif

}  // namespace
}  // namespace shipping
