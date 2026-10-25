#include "shipping_label.hpp"

#include <format>

namespace pool24 {

RenderedLabel render_label(const ShippingLabelRequest& request) {
    std::string text = std::format("SHIP TO: {}\nWEIGHT: {:.2f} kg\nREF: {}", request.destination_city,
                                    request.weight_kg, request.shipment_id);
    return RenderedLabel{.shipment_id = request.shipment_id, .label_text = std::move(text)};
}

}  // namespace pool24
