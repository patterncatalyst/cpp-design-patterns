#pragma once

#include <memory>
#include <string>

#include "shipping_domain.hpp"

namespace shipping {

// PImpl idiom: this header declares Impl but never defines it, so no
// formatting detail leaks into it. A caller including this file sees only
// the public operations; changing how a label is laid out never forces a
// recompile of anything that only calls render().
class LabelRenderer {
public:
    explicit LabelRenderer(std::string carrier_name);
    ~LabelRenderer();

    LabelRenderer(const LabelRenderer& other);
    LabelRenderer& operator=(const LabelRenderer& other);
    LabelRenderer(LabelRenderer&& other) noexcept;
    LabelRenderer& operator=(LabelRenderer&& other) noexcept;

    std::string render(const Shipment& shipment, const std::string& tracking_number) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace shipping
