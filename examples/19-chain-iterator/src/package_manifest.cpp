#include "package_manifest.hpp"

#include <concepts>
#include <ranges>
#include <utility>

namespace shipping {

void PackageManifest::add(Package package) {
    buckets_by_zone_[static_cast<std::size_t>(package.zone)].push_back(std::move(package));
}

std::size_t PackageManifest::size() const {
    std::size_t total = 0;
    for (const auto& bucket : buckets_by_zone_) {
        total += bucket.size();
    }
    return total;
}

PackageManifest::const_iterator::const_iterator(const std::array<std::vector<Package>, kZoneCount>* buckets,
                                                  std::size_t bucket_index, std::size_t position)
    : buckets_(buckets), bucket_index_(bucket_index), position_(position) {
    skip_empty_buckets();
}

void PackageManifest::const_iterator::skip_empty_buckets() {
    while (buckets_ != nullptr && bucket_index_ < buckets_->size() &&
           position_ >= (*buckets_)[bucket_index_].size()) {
        ++bucket_index_;
        position_ = 0;
    }
}

PackageManifest::const_iterator::reference PackageManifest::const_iterator::operator*() const {
    return (*buckets_)[bucket_index_][position_];
}

PackageManifest::const_iterator::pointer PackageManifest::const_iterator::operator->() const {
    return &(*buckets_)[bucket_index_][position_];
}

PackageManifest::const_iterator& PackageManifest::const_iterator::operator++() {
    ++position_;
    skip_empty_buckets();
    return *this;
}

PackageManifest::const_iterator PackageManifest::const_iterator::operator++(int) {
    auto tmp = *this;
    ++(*this);
    return tmp;
}

PackageManifest::const_iterator PackageManifest::begin() const {
    return const_iterator(&buckets_by_zone_, 0, 0);
}

PackageManifest::const_iterator PackageManifest::end() const {
    return const_iterator(&buckets_by_zone_, kZoneCount, 0);
}

// const_iterator satisfies the standard library's forward_iterator concept
// (not just "duck types" like one), and PackageManifest itself satisfies
// forward_range, which is exactly what lets a ranges pipeline operate on a
// manifest directly in package_manifest_test.cpp and demo_main.cpp without
// any adapter code.
static_assert(std::forward_iterator<PackageManifest::const_iterator>);
static_assert(std::ranges::forward_range<PackageManifest>);

}  // namespace shipping
