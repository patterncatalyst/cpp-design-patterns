#pragma once

// Adapt Parameter. Before this technique, WarehouseDispatcher::dispatch()
// took `const CarrierManifestFile&` directly, so every test needed a real
// file on disk just to call it, even though dispatch() only ever reads two
// values back out of the manifest. ManifestSource names that minimal
// surface; CarrierManifestFileAdapter wraps the legacy type so it
// satisfies the new, smaller interface with no change to
// CarrierManifestFile itself.

#include <string>

#include "carrier_manifest_file.hpp"

namespace breakdep28 {

class ManifestSource {
public:
    virtual ~ManifestSource() = default;
    virtual int shipment_count() const = 0;
    virtual std::string carrier_code() const = 0;
};

// The adapter: implements ManifestSource by delegating to a real
// CarrierManifestFile. Production code constructs one of these around a
// parsed manifest; nothing about CarrierManifestFile's own interface
// changes to make this possible.
class CarrierManifestFileAdapter : public ManifestSource {
public:
    explicit CarrierManifestFileAdapter(const CarrierManifestFile& file) : file_(file) {}
    int shipment_count() const override { return file_.shipment_count(); }
    std::string carrier_code() const override { return file_.carrier_code(); }

private:
    const CarrierManifestFile& file_;
};

struct DispatchSummary {
    std::string carrier_code;
    int shipment_count = 0;
    bool large_batch = false;
};

// WarehouseDispatcher now depends on ManifestSource, the interface,
// instead of CarrierManifestFile, the concrete heavyweight type.
class WarehouseDispatcher {
public:
    explicit WarehouseDispatcher(int large_batch_threshold) : large_batch_threshold_(large_batch_threshold) {}
    DispatchSummary dispatch(const ManifestSource& manifest) const;

private:
    int large_batch_threshold_;
};

}  // namespace breakdep28
