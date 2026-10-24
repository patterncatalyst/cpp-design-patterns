#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#if __cplusplus >= 202302L
#include <expected>
#endif

namespace shipping {

// Proxy target interface. A proxy implements the exact same interface as
// the real subject it stands in for, so a client holding a ManifestStore&
// cannot tell whether it is talking to the real archive, a lazy proxy
// deferring construction of that archive, an access-control proxy guarding
// it, or a stack of several proxies wrapping each other.
class ManifestStore {
public:
    virtual ~ManifestStore() = default;
    virtual std::string lookup(const std::string& tracking_number) = 0;
};

// The real subject: expensive to construct, because it loads an entire
// day's manifests off cold storage before it can answer a single lookup.
// This example stands that cost in with a small fixed index; the
// construction itself, not its size, is what LazyManifestStoreProxy below
// defers.
class ArchivedManifestStore : public ManifestStore {
public:
    ArchivedManifestStore();
    std::string lookup(const std::string& tracking_number) override;

private:
    std::unordered_map<std::string, std::string> manifests_;
};

// Proxy: a lazy-initialization surrogate. It implements ManifestStore
// without owning an ArchivedManifestStore up front, and only constructs
// one the first time lookup() is actually called. A caller holding a
// ManifestStore& cannot tell the difference between this proxy and a real
// archive that happened to load instantly; is_loaded() exists only so this
// example's tests can observe the deferral directly.
class LazyManifestStoreProxy : public ManifestStore {
public:
    std::string lookup(const std::string& tracking_number) override;
    bool is_loaded() const { return static_cast<bool>(real_); }

private:
    std::unique_ptr<ArchivedManifestStore> real_;
};

// A caller's clearance to read a manifest. Ordered so a plain `<`
// comparison expresses "below the required level."
enum class AccessLevel {
    Guest,
    Warehouse,
    Admin,
};

// Proxy: an access-control surrogate. It implements ManifestStore and
// holds a real one (which may itself be a LazyManifestStoreProxy), but
// only forwards a lookup when the caller's clearance meets the required
// level. Unlike a Decorator, this proxy does not add a new capability to
// what a manifest lookup returns; it controls whether the call reaches the
// real subject at all.
class AccessControlledManifestProxy : public ManifestStore {
public:
    AccessControlledManifestProxy(std::unique_ptr<ManifestStore> real, AccessLevel caller_level);

    // C++20 contract: throws when the caller's clearance is below what a
    // manifest lookup requires.
    std::string lookup(const std::string& tracking_number) override;

#if __cplusplus >= 202302L
    // C++23 refinement: the same access check, reported as a typed error
    // instead of an exception.
    std::expected<std::string, std::string> try_lookup(const std::string& tracking_number);
#endif

private:
    std::unique_ptr<ManifestStore> real_;
    AccessLevel caller_level_;
    static constexpr AccessLevel kRequiredLevel = AccessLevel::Warehouse;
};

}  // namespace shipping
