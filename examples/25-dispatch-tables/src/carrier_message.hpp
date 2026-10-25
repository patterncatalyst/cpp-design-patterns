#pragma once

// The two message shapes this example dispatches.
//
//   - CarrierMessage carries a CarrierMessageType, a closed enum this
//     codebase controls and compiles against. It is the shape used by the
//     jump table and the constexpr array table, this chapter's two
//     closed-set forms.
//   - CarrierWireMessage carries a wire_tag, a plain string this codebase
//     does not control: a new carrier can introduce a tag nobody here has
//     seen before, without a recompile. It is the shape used by the
//     dispatch map, this chapter's open-set form.
//
// Keeping these as two distinct types, rather than one struct with an
// optional enum, keeps each dispatch form honest about what it actually
// knows at compile time.

#include <cstddef>
#include <string>

#if __cplusplus >= 202302L
#include <utility>
#endif

namespace dispatch25 {

enum class CarrierMessageType {
    Created,
    PickedUp,
    InTransit,
    OutForDelivery,
    Delivered,
    DeliveryException,
};

inline constexpr std::size_t kCarrierMessageTypeCount = 6;

struct CarrierMessage {
    CarrierMessageType type;
    std::string tracking_id;
    std::string detail;
};

struct CarrierWireMessage {
    std::string wire_tag;
    std::string tracking_id;
    std::string detail;
};

// Converts a CarrierMessageType to the positional index both closed-set
// dispatch forms in this chapter index into: the jump table and the
// constexpr array table. C++23's std::to_underlying names the conversion
// to the enum's underlying type explicitly; C++20 has only a bare
// static_cast, which says "convert to std::size_t" and leaves "by way of
// the enum's underlying type" implicit.
inline constexpr std::size_t message_type_index(CarrierMessageType type) {
#if __cplusplus >= 202302L
    return static_cast<std::size_t>(std::to_underlying(type));
#else
    return static_cast<std::size_t>(type);
#endif
}

}  // namespace dispatch25
