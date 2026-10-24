#include "abstract_factory.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

namespace carrier_integration {
namespace {

Shipment sample_shipment() {
    return Shipment{/*destination_country=*/"CA", /*weight_kg=*/4.0, /*declared_value_cents=*/18000};
}

TEST(FastShipIntegrationFactoryTest, YieldsAConsistentMatchingFamily) {
    const FastShipIntegrationFactory factory("acct-77");
    const Shipment shipment = sample_shipment();

    const auto label_formatter = factory.make_label_formatter();
    const auto rate_quoter = factory.make_rate_quoter();
    const auto tracking_client = factory.make_tracking_client();

    EXPECT_NE(label_formatter->format_label(shipment).find("acct-77"), std::string::npos);
    EXPECT_NE(tracking_client->tracking_url("TRK-1").find("acct-77"), std::string::npos);
    EXPECT_NE(label_formatter->format_label(shipment).find("FASTSHIP"), std::string::npos);
    EXPECT_NE(tracking_client->tracking_url("TRK-1").find("fastship"), std::string::npos);
    EXPECT_GT(rate_quoter->quote(shipment), 0);
}

TEST(SummitFreightIntegrationFactoryTest, YieldsAConsistentMatchingFamily) {
    const SummitFreightIntegrationFactory factory("acct-42");
    const Shipment shipment = sample_shipment();

    const auto label_formatter = factory.make_label_formatter();
    const auto rate_quoter = factory.make_rate_quoter();
    const auto tracking_client = factory.make_tracking_client();

    EXPECT_NE(label_formatter->format_label(shipment).find("acct-42"), std::string::npos);
    EXPECT_NE(tracking_client->tracking_url("TRK-1").find("acct-42"), std::string::npos);
    EXPECT_NE(label_formatter->format_label(shipment).find("SF-MANIFEST"), std::string::npos);
    EXPECT_NE(tracking_client->tracking_url("TRK-1").find("summitfreight"), std::string::npos);
    EXPECT_GT(rate_quoter->quote(shipment), 0);
}

TEST(FastShipIntegrationFactoryTest, PricesHigherForAHeavierShipmentThanSummitFreightsBaseline) {
    // Not a claim that FastShip is always more expensive; a concrete fact
    // about the two rate formulas at this one declared shipment, enough to
    // show the two families compute independently.
    const FastShipIntegrationFactory fastship_factory("acct-77");
    const SummitFreightIntegrationFactory summit_factory("acct-42");
    const Shipment shipment{/*destination_country=*/"CA", /*weight_kg=*/1.0, /*declared_value_cents=*/500};

    const auto fastship_quote = fastship_factory.make_rate_quoter()->quote(shipment);
    const auto summit_quote = summit_factory.make_rate_quoter()->quote(shipment);

    EXPECT_EQ(fastship_quote, 530);
    EXPECT_EQ(summit_quote, 440);
}

TEST(ShipmentProcessorTest, BuildsAllThreeProductsFromTheSameFactory) {
    const FastShipIntegrationFactory factory("acct-77");
    const ShipmentProcessor processor(factory);
    const Shipment shipment = sample_shipment();

    const ProcessedShipment processed = processor.process(shipment, "TRK-9");

    EXPECT_NE(processed.label.find("FASTSHIP"), std::string::npos);
    EXPECT_NE(processed.tracking_url.find("fastship.example"), std::string::npos);
    EXPECT_GT(processed.rate_cents, 0);
}

TEST(ShipmentProcessorTest, NeverMixesProductsAcrossTwoDifferentProcessorInstances) {
    const FastShipIntegrationFactory fastship_factory("acct-77");
    const SummitFreightIntegrationFactory summit_factory("acct-42");
    const ShipmentProcessor fastship_processor(fastship_factory);
    const ShipmentProcessor summit_processor(summit_factory);
    const Shipment shipment = sample_shipment();

    const ProcessedShipment fastship_result = fastship_processor.process(shipment, "TRK-1");
    const ProcessedShipment summit_result = summit_processor.process(shipment, "TRK-2");

    EXPECT_NE(fastship_result.label.find("FASTSHIP"), std::string::npos);
    EXPECT_EQ(fastship_result.label.find("SF-MANIFEST"), std::string::npos);
    EXPECT_NE(summit_result.label.find("SF-MANIFEST"), std::string::npos);
    EXPECT_EQ(summit_result.label.find("FASTSHIP"), std::string::npos);
}

TEST(MakeCarrierFactoryTest, LooksUpFastShipByNameCaseInsensitively) {
    const auto factory = make_carrier_factory("FastShip", "acct-77");

    ASSERT_NE(factory, nullptr);
    const auto label = factory->make_label_formatter()->format_label(sample_shipment());
    EXPECT_NE(label.find("FASTSHIP"), std::string::npos);
}

TEST(MakeCarrierFactoryTest, ThrowsOnAnUnknownCarrierName) {
    EXPECT_THROW(make_carrier_factory("globex-shipping", "acct-01"), std::invalid_argument);
}

#if __cplusplus >= 202302L
TEST(TryMakeCarrierFactoryTest, ReturnsATypedErrorForAnUnknownCarrierName) {
    const auto result = try_make_carrier_factory("globex-shipping", "acct-01");

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("globex-shipping"), std::string::npos);
}

TEST(TryMakeCarrierFactoryTest, ReturnsAFactoryForAKnownCarrierName) {
    const auto result = try_make_carrier_factory("summitfreight", "acct-42");

    ASSERT_TRUE(result.has_value());
    const auto label = (*result)->make_label_formatter()->format_label(sample_shipment());
    EXPECT_NE(label.find("SF-MANIFEST"), std::string::npos);
}
#endif

}  // namespace
}  // namespace carrier_integration
