#pragma once

#include <memory>
#include <string>
#include <vector>

#include "exception_handler.hpp"
#include "sku_catalog.hpp"

namespace palletizing {

// The uniform interface a pallet, a carton, and a single item all
// implement. A caller that holds a PackageComponent never needs to know
// whether it is looking at one item or an entire pallet; both answer
// total_weight_grams() and item_count() the same way.
class PackageComponent {
public:
    virtual ~PackageComponent() = default;
    virtual int total_weight_grams() const = 0;
    virtual int item_count() const = 0;
    virtual const std::string& label() const = 0;
};

// The leaf. An Item holds a shared SkuDescriptor flyweight plus the
// extrinsic state that varies per line: how many units, and where this
// particular line is headed. Two Items for the same SKU share one
// descriptor pointer; they never share a quantity or a destination.
class Item : public PackageComponent {
public:
    Item(std::string label, std::shared_ptr<const SkuDescriptor> sku, int quantity, std::string destination);

    int total_weight_grams() const override;
    int item_count() const override;
    const std::string& label() const override { return label_; }

    const SkuDescriptor& sku() const { return *sku_; }
    const std::string& destination() const { return destination_; }

private:
    std::string label_;
    std::shared_ptr<const SkuDescriptor> sku_;  // intrinsic state, shared
    int quantity_;                              // extrinsic state, per line
    std::string destination_;                   // extrinsic state, per line
};

// The composite. A PackageGroup is a carton or a pallet: either one is
// "some children, queried the same way a leaf is queried." Nesting a
// PackageGroup inside another PackageGroup (a carton on a pallet, or a
// carton inside a carton inside a pallet) is exactly how a mixed tree gets
// built: add() takes any PackageComponent, leaf or composite, with no
// special case for which kind it is.
class PackageGroup : public PackageComponent {
public:
    explicit PackageGroup(std::string label,
                           const ShipmentExceptionHandler& handler = null_shipment_exception_handler());

    void add(std::shared_ptr<PackageComponent> child);

    int total_weight_grams() const override;
    int item_count() const override;
    const std::string& label() const override { return label_; }

    // Reports to handler_ if this group's total weight exceeds grams_limit.
    // This call site never checks whether handler_ is "real": it was bound
    // at construction, defaulting to the Null Object above, so the call
    // below is unconditional and branch-free regardless of which handler a
    // caller supplied.
    void check_weight_limit(int grams_limit) const;

private:
    std::string label_;
    std::vector<std::shared_ptr<PackageComponent>> children_;
    const ShipmentExceptionHandler& handler_;
};

}  // namespace palletizing
