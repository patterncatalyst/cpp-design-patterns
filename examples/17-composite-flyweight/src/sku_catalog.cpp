#include "sku_catalog.hpp"

#include <stdexcept>
#include <utility>

namespace palletizing {

void SkuCatalog::register_sku(SkuDescriptor descriptor) {
    const std::string sku_code = descriptor.sku_code;
    master_table_.insert_or_assign(sku_code, std::move(descriptor));
}

std::shared_ptr<const SkuDescriptor> SkuCatalog::get(std::string_view sku_code) {
    const std::string key(sku_code);

    if (const auto pooled = flyweights_.find(key); pooled != flyweights_.end()) {
        return pooled->second;
    }

    const auto registered = master_table_.find(key);
    if (registered == master_table_.end()) {
        throw std::out_of_range("unregistered SKU code: " + key);
    }

    auto flyweight = std::make_shared<const SkuDescriptor>(registered->second);
    flyweights_.insert_or_assign(key, flyweight);
    return flyweight;
}

std::size_t SkuCatalog::pool_size() const {
    return flyweights_.size();
}

#if __cplusplus >= 202302L
std::expected<std::shared_ptr<const SkuDescriptor>, std::string> SkuCatalog::try_get(std::string_view sku_code) {
    const std::string key(sku_code);

    if (const auto pooled = flyweights_.find(key); pooled != flyweights_.end()) {
        return pooled->second;
    }

    const auto registered = master_table_.find(key);
    if (registered == master_table_.end()) {
        return std::unexpected<std::string>("unregistered SKU code: " + key);
    }

    auto flyweight = std::make_shared<const SkuDescriptor>(registered->second);
    flyweights_.insert_or_assign(key, flyweight);
    return flyweight;
}
#endif

}  // namespace palletizing
