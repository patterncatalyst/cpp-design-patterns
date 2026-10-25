#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace pool24 {

// A connection to the shipment-tracking store. Standing one of these up is
// the expensive step this chapter's pool exists to amortize: in a real
// deployment it would open a TLS session and authenticate against the
// store; here it just costs a constructor call the test suite can count,
// which is enough to prove the pool reuses connections instead of
// rebuilding one per request.
class TrackingStoreConnection {
public:
    explicit TrackingStoreConnection(int connection_id);

    int connection_id() const { return connection_id_; }

    // Records a tracking event against this connection and returns an
    // opaque sequence number the store assigned it.
    std::uint64_t record_event(std::string_view shipment_id, std::string_view status);

    std::size_t events_recorded() const { return events_recorded_; }

private:
    int connection_id_;
    std::size_t events_recorded_ = 0;
};

}  // namespace pool24
