#include "manifest_proxy.hpp"

#include <stdexcept>
#include <utility>

namespace shipping {

ArchivedManifestStore::ArchivedManifestStore() {
    // Stands in for reading a day's manifests off cold storage: by the time
    // this constructor returns, every tracking number for the day is
    // already indexed in memory.
    manifests_.emplace("TRK-1001", "FASTSHIP-MANIFEST acct=acct-fastship-77 dest=CA");
    manifests_.emplace("TRK-2002", "SF-MANIFEST account:acct-summit-42 to:CA");
}

std::string ArchivedManifestStore::lookup(const std::string& tracking_number) {
    const auto found = manifests_.find(tracking_number);
    if (found == manifests_.end()) {
        return "no manifest on file for " + tracking_number;
    }
    return found->second;
}

std::string LazyManifestStoreProxy::lookup(const std::string& tracking_number) {
    if (!real_) {
        real_ = std::make_unique<ArchivedManifestStore>();
    }
    return real_->lookup(tracking_number);
}

AccessControlledManifestProxy::AccessControlledManifestProxy(std::unique_ptr<ManifestStore> real,
                                                               AccessLevel caller_level)
    : real_(std::move(real)), caller_level_(caller_level) {}

std::string AccessControlledManifestProxy::lookup(const std::string& tracking_number) {
    if (caller_level_ < kRequiredLevel) {
        throw std::runtime_error("access denied: manifest lookup requires warehouse clearance");
    }
    return real_->lookup(tracking_number);
}

#if __cplusplus >= 202302L
std::expected<std::string, std::string> AccessControlledManifestProxy::try_lookup(
    const std::string& tracking_number) {
    if (caller_level_ < kRequiredLevel) {
        return std::unexpected<std::string>("access denied: manifest lookup requires warehouse clearance");
    }
    return real_->lookup(tracking_number);
}
#endif

}  // namespace shipping
