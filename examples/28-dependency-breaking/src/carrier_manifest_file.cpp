#include "carrier_manifest_file.hpp"

#include <fstream>
#include <stdexcept>

namespace breakdep28 {

CarrierManifestFile::CarrierManifestFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("CarrierManifestFile: cannot open manifest file: " + path);
    }
    std::getline(in, carrier_code_);
    std::string count_line;
    std::getline(in, count_line);
    shipment_count_ = std::stoi(count_line);
}

}  // namespace breakdep28
