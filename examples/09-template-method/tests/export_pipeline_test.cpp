#include "export_pipeline.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace orderexport {
namespace {

Order sample_order() {
    return Order{"ORD-77", "Riverside Hardware Co.", {OrderLine{"SKU-100", 3}, OrderLine{"SKU-205", 1}}};
}

Order empty_order() {
    return Order{"ORD-00", "Nobody", {}};
}

// A test-only exporter that records the order every hook is called in,
// so the suite can assert export_order()'s fixed call sequence directly
// instead of only inferring it from the finished document. It never
// overrides export_order() itself, because export_order() is not
// virtual: NVI's whole point is that a subclass has no way to change the
// skeleton, only to supply what the three hooks below produce.
class RecordingExporter : public OrderExportPipeline {
public:
    mutable std::vector<std::string> calls;

private:
    std::string_view format_name() const override {
        calls.push_back("format_name");
        return "RECORDING";
    }

    std::string render_header(const Order& /*order*/) const override {
        calls.push_back("render_header");
        return "H\n";
    }

    std::string render_line(const OrderLine& line) const override {
        calls.push_back("render_line:" + line.sku);
        return "L:" + line.sku + "\n";
    }

    std::string render_footer(const Order& /*order*/) const override {
        calls.push_back("render_footer");
        return "F\n";
    }
};

TEST(OrderExportPipelineTest, CallsHooksInTheFixedSkeletonOrder) {
    RecordingExporter exporter;
    const Order order = sample_order();

    exporter.export_order(order);

    const std::vector<std::string> expected = {
        "render_header",
        "render_line:SKU-100",
        "render_line:SKU-205",
        "render_footer",
        "format_name",  // called from trailer_for(), after render_footer()
    };
    EXPECT_EQ(exporter.calls, expected);
}

TEST(OrderExportPipelineTest, RejectsAnOrderWithNoLineItemsBeforeAnyHookRuns) {
    RecordingExporter exporter;

    EXPECT_THROW(exporter.export_order(empty_order()), std::invalid_argument);
    EXPECT_TRUE(exporter.calls.empty()) << "validate() must reject the order before any hook is called";
}

TEST(OrderExportPipelineTest, AppendsTheTrailerRegardlessOfWhatRenderFooterProduced) {
    // render_footer() returns a fixed "F\n" here and never touches the
    // trailer; the trailer line still shows up last, because trailer_for()
    // is not a hook a subclass can suppress or replace.
    RecordingExporter exporter;

    const std::string document = exporter.export_order(sample_order());

    EXPECT_TRUE(document.ends_with("-- 4 unit(s) exported via RECORDING --\n")) << document;
}

TEST(CsvManifestExporterTest, ProducesTheExpectedManifest) {
    CsvManifestExporter exporter;

    const std::string document = exporter.export_order(sample_order());

    EXPECT_EQ(document,
              "order_id,customer_name\n"
              "ORD-77,Riverside Hardware Co.\n"
              "sku,quantity\n"
              "SKU-100,3\n"
              "SKU-205,1\n"
              "-- 4 unit(s) exported via CSV --\n");
}

TEST(JsonManifestExporterTest, ProducesOneJsonObjectPerLineWithNoWrappingArray) {
    JsonManifestExporter exporter;

    const std::string document = exporter.export_order(sample_order());

    EXPECT_EQ(document,
              "{\"order_id\":\"ORD-77\",\"customer_name\":\"Riverside Hardware Co.\"}\n"
              "{\"sku\":\"SKU-100\",\"quantity\":3}\n"
              "{\"sku\":\"SKU-205\",\"quantity\":1}\n"
              "-- 4 unit(s) exported via JSON --\n");
}

TEST(FixedWidthEdiExporterTest, ProducesPaddedFixedWidthSegments) {
    FixedWidthEdiExporter exporter;

    const std::string document = exporter.export_order(sample_order());

    EXPECT_EQ(document,
              "ORDORD-77      Riverside Hardware Co.  \n"
              "LINSKU-100     00003\n"
              "LINSKU-205     00001\n"
              "END00002\n"
              "-- 4 unit(s) exported via fixed-width EDI --\n");
}

TEST(OrderExportPipelineTest, EveryFormatRejectsAnEmptyOrderIdentically) {
    CsvManifestExporter csv_exporter;
    JsonManifestExporter json_exporter;
    FixedWidthEdiExporter edi_exporter;

    EXPECT_THROW(csv_exporter.export_order(empty_order()), std::invalid_argument);
    EXPECT_THROW(json_exporter.export_order(empty_order()), std::invalid_argument);
    EXPECT_THROW(edi_exporter.export_order(empty_order()), std::invalid_argument);
}

}  // namespace
}  // namespace orderexport
