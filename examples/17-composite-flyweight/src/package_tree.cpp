#include "package_tree.hpp"

#include <numeric>
#include <utility>

namespace palletizing {

Item::Item(std::string label, std::shared_ptr<const SkuDescriptor> sku, int quantity, std::string destination)
    : label_(std::move(label)), sku_(std::move(sku)), quantity_(quantity), destination_(std::move(destination)) {}

int Item::total_weight_grams() const {
    return sku_->unit_weight_grams * quantity_;
}

int Item::item_count() const {
    return quantity_;
}

PackageGroup::PackageGroup(std::string label, const ShipmentExceptionHandler& handler)
    : label_(std::move(label)), handler_(handler) {}

void PackageGroup::add(std::shared_ptr<PackageComponent> child) {
    children_.push_back(std::move(child));
}

int PackageGroup::total_weight_grams() const {
    return std::accumulate(children_.begin(), children_.end(), 0,
                            [](int sum, const std::shared_ptr<PackageComponent>& child) {
                                return sum + child->total_weight_grams();
                            });
}

int PackageGroup::item_count() const {
    return std::accumulate(children_.begin(), children_.end(), 0,
                            [](int sum, const std::shared_ptr<PackageComponent>& child) {
                                return sum + child->item_count();
                            });
}

void PackageGroup::check_weight_limit(int grams_limit) const {
    const int total = total_weight_grams();
    if (total > grams_limit) {
        handler_.on_overweight(label_, total - grams_limit);
    }
}

}  // namespace palletizing
