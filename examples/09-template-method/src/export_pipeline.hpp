#pragma once

#include <string>
#include <string_view>

#include "order_domain.hpp"

namespace orderexport {

// Template Method plus the Non-Virtual Interface (NVI) idiom. export_order()
// is the one public entry point and it is never virtual, so the order a
// warehouse system exports a document in, validate, header, one line per
// item, footer, trailer, is fixed for every format and cannot be
// reordered or skipped by a subclass. The three steps a concrete format
// needs to customize are private virtual hooks, called only from inside
// export_order() itself; nothing outside this class, and nothing in a
// subclass either, can call a hook directly.
class OrderExportPipeline {
public:
    virtual ~OrderExportPipeline() = default;

    // The fixed skeleton. A caller holding any concrete exporter through
    // this base class always gets the same sequence of steps; only the
    // text three of those steps produce differs between formats.
    std::string export_order(const Order& order) const;

private:
    // Precondition shared by every format: an order with nothing to
    // export is a programming error at this boundary, not a format
    // choice, so it is not a hook. No subclass can override it, and
    // every subclass gets this check for free.
    static void validate(const Order& order);

    // Postcondition shared by every format: a trailer line naming the
    // format and the total unit count, computed from the one shared
    // total_quantity() function so the reported count can never drift
    // from the order's actual lines. Not a hook either; it runs after
    // render_footer() no matter what render_footer() did.
    std::string trailer_for(const Order& order) const;

    // Hooks: private virtual. Each concrete exporter below overrides all
    // three. Private, not protected, is NVI's second half: a subclass
    // supplies these steps but has no way to call them itself, and no
    // caller outside this class can reach them at all.
    virtual std::string_view format_name() const = 0;
    virtual std::string render_header(const Order& order) const = 0;
    virtual std::string render_line(const OrderLine& line) const = 0;
    virtual std::string render_footer(const Order& order) const = 0;
};

// Comma-separated manifest: a two-line header naming the order, then one
// row per line item.
class CsvManifestExporter : public OrderExportPipeline {
private:
    std::string_view format_name() const override;
    std::string render_header(const Order& order) const override;
    std::string render_line(const OrderLine& line) const override;
    std::string render_footer(const Order& order) const override;
};

// Newline-delimited JSON: one JSON object per line, order metadata first,
// no wrapping array and no comma bookkeeping between elements.
class JsonManifestExporter : public OrderExportPipeline {
private:
    std::string_view format_name() const override;
    std::string render_header(const Order& order) const override;
    std::string render_line(const OrderLine& line) const override;
    std::string render_footer(const Order& order) const override;
};

// Fixed-width EDI-style segments: a three-letter segment code followed by
// space-padded or zero-padded columns, the shape a legacy trading-partner
// feed still expects from a warehouse system today.
class FixedWidthEdiExporter : public OrderExportPipeline {
private:
    std::string_view format_name() const override;
    std::string render_header(const Order& order) const override;
    std::string render_line(const OrderLine& line) const override;
    std::string render_footer(const Order& order) const override;
};

}  // namespace orderexport
