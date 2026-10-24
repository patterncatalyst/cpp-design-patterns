#pragma once

namespace crtp21 {

// CRTP, expression two: a reusable mixin, not a rate model at all, to show
// the self-type trick is a general-purpose tool rather than something
// specific to RateModel above. Any Derived that exposes a sort_key()
// method can inherit Comparable<Derived> and get operator<, operator==,
// and operator> for free. The friend functions below are found by
// argument-dependent lookup on Derived, so they work across completely
// unrelated Derived types with no shared base beyond Comparable itself,
// and with no virtual dispatch anywhere.
template <typename Derived>
class Comparable {
public:
    friend constexpr bool operator<(const Derived& lhs, const Derived& rhs) {
        return lhs.sort_key() < rhs.sort_key();
    }

    friend constexpr bool operator==(const Derived& lhs, const Derived& rhs) {
        return lhs.sort_key() == rhs.sort_key();
    }

    friend constexpr bool operator>(const Derived& lhs, const Derived& rhs) { return rhs < lhs; }
};

}  // namespace crtp21
