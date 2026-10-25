#pragma once

// Form two: a dispatch map for the open set. The wire_tag a carrier sends
// is a string this codebase does not control, so a closed enum cannot
// represent it. MessageDispatchMap seeds the wire tags this codebase
// already knows about in its constructor, and register_handler() lets a
// new carrier's tag be added afterward, at runtime, with no change to this
// class, to any caller that already resolved an earlier tag, or to the
// CarrierMessageType enum the closed-set forms depend on.

#include <functional>
#include <string>
#include <unordered_map>

#include "carrier_message.hpp"
#include "tracking_ledger.hpp"

#if __cplusplus >= 202302L
#include <flat_map>
#endif

namespace dispatch25 {

class MessageDispatchMap {
public:
    using Handler = std::function<void(const CarrierWireMessage&, TrackingSink&)>;

    MessageDispatchMap();

    void register_handler(std::string wire_tag, Handler handler);
    void dispatch(const CarrierWireMessage& message, TrackingSink& sink) const;

    std::size_t handler_count() const { return handlers_.size(); }

private:
    std::unordered_map<std::string, Handler> handlers_;
};

#if __cplusplus >= 202302L
// A C++23 refinement of the same open contract. Registration happens once
// per carrier, at startup or when a new carrier is onboarded; dispatch
// runs once per inbound message, the hot path. That write-rarely,
// read-often profile is exactly what std::flat_map trades for: its entries
// live in two contiguous, sorted vectors instead of a node-based hash
// table, so a lookup is a cache-friendly binary search with no per-entry
// heap allocation and no hash computation.
class FlatMessageDispatchMap {
public:
    using Handler = std::function<void(const CarrierWireMessage&, TrackingSink&)>;

    FlatMessageDispatchMap();

    void register_handler(std::string wire_tag, Handler handler);
    void dispatch(const CarrierWireMessage& message, TrackingSink& sink) const;

    std::size_t handler_count() const { return handlers_.size(); }

private:
    std::flat_map<std::string, Handler> handlers_;
};
#endif

}  // namespace dispatch25
