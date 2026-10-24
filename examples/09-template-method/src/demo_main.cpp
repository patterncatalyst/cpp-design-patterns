// A warehouse order exported three idiomatic document formats, all
// through the same fixed Template Method skeleton: validate, header, one
// line per item, footer, trailer. Only the three middle hooks differ
// between formats.
//
// Build and run via scripts/build.sh from the repo root, or standalone
// from this directory: see README.md "Build, run, observe".

#include <iostream>

#include "export_pipeline.hpp"
#include "order_domain.hpp"
#include "static_dispatch_variant.hpp"

int main() {
    using namespace orderexport;

    const Order order{"ORD-77", "Riverside Hardware Co.", {OrderLine{"SKU-100", 3}, OrderLine{"SKU-205", 1}}};

    std::cout << "== CSV manifest (CsvManifestExporter) ==\n";
    CsvManifestExporter csv_exporter;
    std::cout << csv_exporter.export_order(order);

    std::cout << "\n== Newline-delimited JSON (JsonManifestExporter) ==\n";
    JsonManifestExporter json_exporter;
    std::cout << json_exporter.export_order(order);

    std::cout << "\n== Fixed-width EDI (FixedWidthEdiExporter) ==\n";
    FixedWidthEdiExporter edi_exporter;
    std::cout << edi_exporter.export_order(order);

    std::cout << "\n== Static dispatch: CRTP (C++20) ==\n";
    CrtpOrderSummary crtp_summary;
    std::cout << crtp_summary.summarize(order) << '\n';

#if __cplusplus >= 202302L
    std::cout << "\n== Static dispatch: deducing this (C++23) ==\n";
    DeducedOrderSummary deduced_summary;
    std::cout << deduced_summary.summarize(order) << '\n';
#endif

    return 0;
}
