#include "export_pipeline.hpp"

#include <cstddef>
#include <stdexcept>

namespace orderexport {

namespace {

// Column padding for the fixed-width EDI exporter only. Assumes every
// field fits the column width, the same simplifying assumption this
// example's other two exporters make about their own fields.
std::string pad_right(std::string value, std::size_t width) {
    if (value.size() < width) {
        value.append(width - value.size(), ' ');
    }
    return value;
}

std::string pad_left(std::string value, std::size_t width, char fill) {
    if (value.size() < width) {
        value.insert(value.begin(), width - value.size(), fill);
    }
    return value;
}

}  // namespace

void OrderExportPipeline::validate(const Order& order) {
    if (order.lines.empty()) {
        throw std::invalid_argument("order " + order.order_id + " has no line items to export");
    }
}

std::string OrderExportPipeline::trailer_for(const Order& order) const {
    return "-- " + std::to_string(total_quantity(order)) + " unit(s) exported via " + std::string(format_name()) +
           " --\n";
}

std::string OrderExportPipeline::export_order(const Order& order) const {
    validate(order);

    std::string document = render_header(order);
    for (const OrderLine& line : order.lines) {
        document += render_line(line);
    }
    document += render_footer(order);
    document += trailer_for(order);
    return document;
}

// CsvManifestExporter

std::string_view CsvManifestExporter::format_name() const {
    return "CSV";
}

std::string CsvManifestExporter::render_header(const Order& order) const {
    return "order_id,customer_name\n" + order.order_id + "," + order.customer_name + "\nsku,quantity\n";
}

std::string CsvManifestExporter::render_line(const OrderLine& line) const {
    return line.sku + "," + std::to_string(line.quantity) + "\n";
}

std::string CsvManifestExporter::render_footer(const Order& /*order*/) const {
    return "";
}

// JsonManifestExporter

std::string_view JsonManifestExporter::format_name() const {
    return "JSON";
}

std::string JsonManifestExporter::render_header(const Order& order) const {
    return "{\"order_id\":\"" + order.order_id + "\",\"customer_name\":\"" + order.customer_name + "\"}\n";
}

std::string JsonManifestExporter::render_line(const OrderLine& line) const {
    return "{\"sku\":\"" + line.sku + "\",\"quantity\":" + std::to_string(line.quantity) + "}\n";
}

std::string JsonManifestExporter::render_footer(const Order& /*order*/) const {
    return "";
}

// FixedWidthEdiExporter

std::string_view FixedWidthEdiExporter::format_name() const {
    return "fixed-width EDI";
}

std::string FixedWidthEdiExporter::render_header(const Order& order) const {
    return "ORD" + pad_right(order.order_id, 12) + pad_right(order.customer_name, 24) + "\n";
}

std::string FixedWidthEdiExporter::render_line(const OrderLine& line) const {
    return "LIN" + pad_right(line.sku, 12) + pad_left(std::to_string(line.quantity), 5, '0') + "\n";
}

std::string FixedWidthEdiExporter::render_footer(const Order& order) const {
    return "END" + pad_left(std::to_string(order.lines.size()), 5, '0') + "\n";
}

}  // namespace orderexport
