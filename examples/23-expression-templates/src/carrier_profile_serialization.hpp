#pragma once

#include <string>
#include <string_view>
#include <tuple>

namespace exprtmpl {

// Templated serialization. Chapter 13's serialize_manifest() and
// deserialize_manifest() each hand-write one std::format() call, or one
// parse step, per field, in two separate functions that have to be kept in
// sync by hand as the domain grows a field. Here, exactly one function,
// describe_fields(), lists every field once as a (name, member pointer)
// pair; serialize() and deserialize() are both generic over that single
// list below, so adding a field to CarrierProfile means editing
// describe_fields() and nothing else, not two separate functions.
struct CarrierProfile {
    std::string carrier_name;
    double base_rate_multiplier = 1.0;
    double max_weight_kg = 0.0;

    // CarrierProfile holds no owned pointers, so a defaulted comparison
    // compares every member directly. Chapter 13's move-only
    // ShipmentManifest holds owning pointers to a polymorphic base and
    // needed a hand-written structural comparison instead; this type needs
    // nothing beyond = default.
    friend bool operator==(const CarrierProfile&, const CarrierProfile&) = default;
};

template <typename Class, typename Member>
struct FieldAccessor {
    std::string_view name;
    Member Class::*member;

    const Member& get(const Class& obj) const { return obj.*member; }
    void set(Class& obj, const Member& value) const { obj.*member = value; }
};

template <typename Class, typename Member>
constexpr FieldAccessor<Class, Member> field(std::string_view name, Member Class::*member) {
    return FieldAccessor<Class, Member>{name, member};
}

// The one describe/visit hook. Every other function in this file walks
// this same tuple; nothing else in the codebase repeats this field list.
constexpr auto describe_fields() {
    return std::make_tuple(field("carrier_name", &CarrierProfile::carrier_name),
                            field("base_rate_multiplier", &CarrierProfile::base_rate_multiplier),
                            field("max_weight_kg", &CarrierProfile::max_weight_kg));
}

std::string serialize(const CarrierProfile& profile);
CarrierProfile deserialize(const std::string& text);

}  // namespace exprtmpl
