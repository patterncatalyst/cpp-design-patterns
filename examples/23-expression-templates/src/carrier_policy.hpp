#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#if __cplusplus >= 202302L
#include <expected>
#endif

namespace exprtmpl {

// Self-registering Prototype registry. Chapter 13's PackageTemplateRegistry
// is populated by a caller who hand-writes a register_template() call for
// every package kind; adding a kind there means remembering to touch that
// call site. Here, a concrete CarrierPolicy registers itself into the
// registry at static-initialization time, through the small
// CarrierPolicyRegistrar helper below, so adding a carrier means adding one
// new .cpp file and nothing else: no central list, no switch, to edit.
class CarrierPolicy {
public:
    virtual ~CarrierPolicy() = default;
    virtual std::unique_ptr<CarrierPolicy> clone() const = 0;
    virtual std::string_view name() const = 0;
    virtual double base_rate_multiplier() const = 0;

protected:
    CarrierPolicy() = default;
    CarrierPolicy(const CarrierPolicy&) = default;
};

class CarrierPolicyRegistry {
public:
    static CarrierPolicyRegistry& instance();

    void register_prototype(std::string key, std::unique_ptr<CarrierPolicy> prototype);
    std::unique_ptr<CarrierPolicy> create(const std::string& key) const;
    std::size_t size() const;

#if __cplusplus >= 202302L
    // C++23 refinement: report WHY a lookup failed (an empty key versus an
    // unknown one) instead of collapsing both cases into a null pointer.
    // Guarded so the mandatory C++20 build path never requires <expected>.
    std::expected<std::unique_ptr<CarrierPolicy>, std::string> try_create(const std::string& key) const;
#endif

private:
    std::unordered_map<std::string, std::unique_ptr<CarrierPolicy>> prototypes_;
};

// One CarrierPolicyRegistrar<Concrete> instance, declared at namespace
// scope inside Concrete's own .cpp file, is the entire registration
// mechanism: its constructor runs during static initialization, before
// main() ever starts, and nothing else anywhere has to know Concrete
// exists for it to show up in the registry.
template <typename Concrete>
class CarrierPolicyRegistrar {
public:
    explicit CarrierPolicyRegistrar(std::string key) {
        CarrierPolicyRegistry::instance().register_prototype(std::move(key), std::make_unique<Concrete>());
    }
};

}  // namespace exprtmpl
