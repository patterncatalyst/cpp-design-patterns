#pragma once

#include <memory>
#include <string>
#include <string_view>

#if __cplusplus >= 202302L
#include <expected>
#include <flat_map>
#else
#include <map>
#endif

namespace palletizing {

// Hazmat classification travels with the SKU, not with any one package: two
// warehouses shipping the same battery model must apply the same handling
// rule, which is exactly the kind of fact a flyweight's intrinsic state is
// for.
enum class HazmatClass { None, Flammable, Corrosive, Lithium };

// Intrinsic state: everything a SKU descriptor carries is true for every
// unit of that SKU, in every carton, on every pallet, forever. Nothing here
// varies per line item, which is what makes this type safe to share by
// pointer instead of copying it into every Item that references it.
struct SkuDescriptor {
    std::string sku_code;
    std::string product_name;
    int unit_weight_grams;
    int length_mm;
    int width_mm;
    int height_mm;
    HazmatClass hazmat_class;
};

// Flyweight factory and pool. A warehouse's product master can list
// thousands of SKUs, but a single pallet only ever touches a handful of
// them; SkuCatalog separates "every SKU this warehouse could ship"
// (registered once, up front) from "every SKU this program has actually
// materialized as a shared flyweight" (populated lazily, on first request).
// get() returns the same std::shared_ptr<const SkuDescriptor> for the same
// sku_code every time, so a thousand Item objects referencing "WIDGET-100"
// hold a thousand copies of one pointer, not a thousand copies of the
// descriptor itself.
class SkuCatalog {
public:
    void register_sku(SkuDescriptor descriptor);

    // Throws std::out_of_range if sku_code was never registered. See
    // try_get() below for the C++23 typed alternative.
    std::shared_ptr<const SkuDescriptor> get(std::string_view sku_code);

    // Number of distinct SKUs actually materialized as flyweights so far.
    // This is the number the Flyweight pattern promises stays small: it
    // grows with the variety of SKUs requested, never with how many times
    // any one of them is requested.
    std::size_t pool_size() const;

#if __cplusplus >= 202302L
    // C++23 refinement: the same lookup, reported as a typed error instead
    // of an exception. Guarded so the mandatory C++20 build path never
    // needs std::expected.
    std::expected<std::shared_ptr<const SkuDescriptor>, std::string> try_get(std::string_view sku_code);
#endif

private:
    // master_table_ is the registered product catalog: every SKU this
    // warehouse knows how to describe. flyweights_ is the pool itself: only
    // the SKUs some caller has actually asked for, each materialized once.
    //
    // Under C++23 the pool is a std::flat_map, a sorted, vector-backed
    // associative container: its keys and values sit in two contiguous
    // buffers instead of a tree of individually allocated nodes, which is
    // friendlier to the cache for the small, rarely-mutated pool a pallet's
    // worth of SKUs produces. The trade shows up on mutation: inserting a
    // new key into a flat_map can reallocate its backing vector and
    // invalidate every existing iterator and reference into it, the way
    // inserting into a std::vector does. A std::map never does that; a
    // node-based tree keeps every existing iterator and reference valid
    // across an insertion elsewhere in the map. Neither map invalidates the
    // std::shared_ptr values get() already handed out, because those
    // pointers own their SkuDescriptor on the heap independent of the map's
    // own storage; the trade-off only matters to code that keeps an
    // iterator into the pool itself across a later registration, which this
    // example's call sites never do.
#if __cplusplus >= 202302L
    std::flat_map<std::string, SkuDescriptor> master_table_;
    std::flat_map<std::string, std::shared_ptr<const SkuDescriptor>> flyweights_;
#else
    std::map<std::string, SkuDescriptor> master_table_;
    std::map<std::string, std::shared_ptr<const SkuDescriptor>> flyweights_;
#endif
};

}  // namespace palletizing
