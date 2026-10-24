#pragma once

#include <array>
#include <cstddef>
#include <iterator>
#include <string>
#include <vector>

namespace shipping {

// The same DestinationZone vocabulary this book has used since the
// Strategy chapter, kept local to this example because every example in
// this repo is standalone (no shared headers across examples/ directories).
enum class DestinationZone {
    Local,
    Regional,
    National,
    International,
};

struct Package {
    std::string tracking_number;
    double weight_kg;
    DestinationZone zone;
};

// Iterator: a warehouse sorts packages into one lane per destination zone
// as they arrive, so PackageManifest stores them bucketed by zone, not as
// one flat vector in arrival order. A caller walking the manifest should
// not need to know that. const_iterator flattens the four zone buckets
// into a single forward traversal in zone order (Local, Regional, National,
// International), so the manifest's internal storage can change shape
// later without changing how a client iterates it.
class PackageManifest {
public:
    static constexpr std::size_t kZoneCount = 4;

    void add(Package package);
    std::size_t size() const;

    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Package;
        using difference_type = std::ptrdiff_t;
        using pointer = const Package*;
        using reference = const Package&;

        const_iterator() = default;

        reference operator*() const;
        pointer operator->() const;
        const_iterator& operator++();
        const_iterator operator++(int);

        bool operator==(const const_iterator& other) const = default;

    private:
        friend class PackageManifest;
        const_iterator(const std::array<std::vector<Package>, kZoneCount>* buckets, std::size_t bucket_index,
                        std::size_t position);

        void skip_empty_buckets();

        const std::array<std::vector<Package>, kZoneCount>* buckets_ = nullptr;
        std::size_t bucket_index_ = 0;
        std::size_t position_ = 0;
    };

    const_iterator begin() const;
    const_iterator end() const;

private:
    std::array<std::vector<Package>, kZoneCount> buckets_by_zone_{};
};

}  // namespace shipping
