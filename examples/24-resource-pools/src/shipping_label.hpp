#pragma once

#include <string>

namespace pool24 {

// A request to render one shipping label, and the rendered result. Both
// are plain, copyable values so they can cross the queue boundary into a
// worker thread (LabelRenderThreadPool) or an active object's own thread
// (LabelPrinterActiveObject) without any lifetime questions.
struct ShippingLabelRequest {
    std::string shipment_id;
    std::string destination_city;
    double weight_kg = 0.0;
};

struct RenderedLabel {
    std::string shipment_id;
    std::string label_text;
};

// Simulates the (moderately expensive, CPU-bound) work of laying out and
// rendering one shipping label. Free from any threading concern on
// purpose: both the thread pool and the active object in this example
// call the same function, from a worker thread either way, so it is the
// shared "what gets executed" the two patterns differ only in *how they
// hand this work off*.
RenderedLabel render_label(const ShippingLabelRequest& request);

}  // namespace pool24
