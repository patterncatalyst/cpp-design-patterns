#include "package_prototype.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

using manifest::PackageTemplateRegistry;

namespace {

PackageTemplateRegistry sample_registry() {
    return manifest::default_package_template_registry();
}

}  // namespace

TEST(PackageTemplateRegistryTest, ClonedPackageStartsWithTheRegisteredPrototypesValues) {
    const PackageTemplateRegistry registry = sample_registry();

    const auto clone = registry.create("fragile-electronics");

    EXPECT_EQ(clone->kind(), "fragile-electronics");
    EXPECT_DOUBLE_EQ(clone->weight_kg(), 3.50);
    EXPECT_EQ(clone->handling_code(), "FRAGILE");
    EXPECT_EQ(clone->declared_value_cents(), 180000);
}

TEST(PackageTemplateRegistryTest, MutatingOneCloneDoesNotTouchTheRegisteredPrototype) {
    const PackageTemplateRegistry registry = sample_registry();

    auto clone = registry.create("bulk-dry-goods");
    clone->set_weight_kg(99.0);
    clone->set_declared_value_cents(500000);

    const auto fresh_clone = registry.create("bulk-dry-goods");

    EXPECT_DOUBLE_EQ(fresh_clone->weight_kg(), 22.00);
    EXPECT_EQ(fresh_clone->declared_value_cents(), 35000);
}

TEST(PackageTemplateRegistryTest, TwoClonesOfTheSamePrototypeAreIndependentOfEachOther) {
    const PackageTemplateRegistry registry = sample_registry();

    auto first = registry.create("refrigerated-perishable");
    auto second = registry.create("refrigerated-perishable");

    first->set_weight_kg(1.0);

    EXPECT_DOUBLE_EQ(first->weight_kg(), 1.0);
    EXPECT_DOUBLE_EQ(second->weight_kg(), 14.75);
}

TEST(PackageTemplateRegistryTest, UnknownTemplateNameThrows) {
    const PackageTemplateRegistry registry = sample_registry();

    EXPECT_THROW(registry.create("nonexistent-template"), std::out_of_range);
}

TEST(PackageTemplateRegistryTest, ContainsReportsRegisteredNamesOnly) {
    const PackageTemplateRegistry registry = sample_registry();

    EXPECT_TRUE(registry.contains("fragile-electronics"));
    EXPECT_FALSE(registry.contains("nonexistent-template"));
}

TEST(MakePackageTest, ConstructsEachKindWithTheRequestedWeightAndValue) {
    const auto electronics = manifest::make_package("fragile-electronics", 2.25, 50000);
    const auto dry_goods = manifest::make_package("bulk-dry-goods", 10.00, 25000);
    const auto perishable = manifest::make_package("refrigerated-perishable", 6.75, 40000);

    EXPECT_EQ(electronics->kind(), "fragile-electronics");
    EXPECT_DOUBLE_EQ(electronics->weight_kg(), 2.25);
    EXPECT_EQ(electronics->declared_value_cents(), 50000);

    EXPECT_EQ(dry_goods->kind(), "bulk-dry-goods");
    EXPECT_EQ(dry_goods->handling_code(), "STANDARD");

    EXPECT_EQ(perishable->kind(), "refrigerated-perishable");
    EXPECT_EQ(perishable->handling_code(), "FROZEN");
}

TEST(MakePackageTest, UnrecognizedKindThrows) {
    EXPECT_THROW(manifest::make_package("unlisted-kind", 1.0, 100), std::invalid_argument);
}
