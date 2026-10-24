#include "carrier_profile_serialization.hpp"

#include <sstream>
#include <unordered_map>

namespace exprtmpl {
namespace {

std::string field_to_text(const std::string& value) { return value; }
std::string field_to_text(double value) { return std::to_string(value); }

void field_from_text(const std::string& text, std::string& out) { out = text; }
void field_from_text(const std::string& text, double& out) { out = std::stod(text); }

std::unordered_map<std::string, std::string> parse_key_value_lines(const std::string& text) {
    std::unordered_map<std::string, std::string> result;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        const auto equals_pos = line.find('=');
        if (equals_pos == std::string::npos) {
            continue;
        }
        result.emplace(line.substr(0, equals_pos), line.substr(equals_pos + 1));
    }
    return result;
}

template <typename Class, typename Member>
void apply_field_from_map(const FieldAccessor<Class, Member>& accessor, Class& obj,
                           const std::unordered_map<std::string, std::string>& values) {
    const auto it = values.find(std::string(accessor.name));
    if (it == values.end()) {
        return;
    }
    Member value{};
    field_from_text(it->second, value);
    accessor.set(obj, value);
}

}  // namespace

std::string serialize(const CarrierProfile& profile) {
    std::string text;
    std::apply(
        [&](const auto&... accessors) {
            ((text += std::string(accessors.name) + "=" + field_to_text(accessors.get(profile)) + "\n"), ...);
        },
        describe_fields());
    return text;
}

CarrierProfile deserialize(const std::string& text) {
    const auto values = parse_key_value_lines(text);
    CarrierProfile profile;
    std::apply([&](const auto&... accessors) { (apply_field_from_map(accessors, profile, values), ...); },
               describe_fields());
    return profile;
}

}  // namespace exprtmpl
