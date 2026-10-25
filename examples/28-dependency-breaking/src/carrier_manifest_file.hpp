#pragma once

// The legacy, heavyweight parameter type this chapter's Adapt Parameter
// pass works around. Constructing one parses a real file from disk; two
// lines are the whole format this chapter cares about (a carrier code,
// then a shipment count), standing in for a much larger legacy parser that
// also validates checksums and resolves carrier-specific encodings.

#include <string>

namespace breakdep28 {

class CarrierManifestFile {
public:
    explicit CarrierManifestFile(const std::string& path);

    int shipment_count() const { return shipment_count_; }
    std::string carrier_code() const { return carrier_code_; }

private:
    std::string carrier_code_;
    int shipment_count_ = 0;
};

}  // namespace breakdep28
