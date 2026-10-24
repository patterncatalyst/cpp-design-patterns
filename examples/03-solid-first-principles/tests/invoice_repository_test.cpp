#include "invoice_repository.hpp"

#include <gtest/gtest.h>

namespace billing {
namespace {

TEST(InMemoryInvoiceRepositoryTest, FindByIdReturnsNulloptWhenNothingIsSaved) {
    const InMemoryInvoiceRepository repository;

    EXPECT_FALSE(repository.find_by_id("missing").has_value());
}

TEST(InMemoryInvoiceRepositoryTest, SaveThenFindByIdRoundTripsTheInvoice) {
    InMemoryInvoiceRepository repository;
    const Invoice invoice{"INV-9", "Example Customer", {{"Consulting", 1, 5000}}};

    repository.save(invoice);
    const auto found = repository.find_by_id("INV-9");

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->customer_name, "Example Customer");
    ASSERT_EQ(found->line_items.size(), 1U);
    EXPECT_EQ(found->line_items[0].description, "Consulting");
}

TEST(InMemoryInvoiceRepositoryTest, SavingASecondInvoiceWithTheSameIdOverwritesTheFirst) {
    InMemoryInvoiceRepository repository;
    repository.save(Invoice{"INV-10", "First Customer", {}});
    repository.save(Invoice{"INV-10", "Second Customer", {}});

    const auto found = repository.find_by_id("INV-10");

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->customer_name, "Second Customer");
}

}  // namespace
}  // namespace billing
