#include "manifest_serialization.hpp"

#include <format>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "package_prototype.hpp"

#if __cplusplus >= 202302L
#include <print>
#endif

namespace manifest {

namespace {

std::string_view trim(std::string_view text) {
    while (!text.empty() && (text.front() == '\r' || text.front() == ' ')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == '\r' || text.back() == ' ')) {
        text.remove_suffix(1);
    }
    return text;
}

std::pair<std::string_view, std::string_view> split_key_value(std::string_view token, char delimiter) {
    const auto pos = token.find(delimiter);
    if (pos == std::string_view::npos) {
        throw std::invalid_argument("deserialize_manifest: malformed token '" + std::string(token) + "'");
    }
    return {token.substr(0, pos), token.substr(pos + 1)};
}

std::unique_ptr<PackageTemplate> parse_package_line(std::string_view line) {
    std::string_view rest = trim(line.substr(std::string_view("PACKAGE").size()));

    std::string kind;
    double weight_kg = 0.0;
    Cents declared_value_cents = 0;
    bool have_kind = false;
    bool have_weight = false;
    bool have_value = false;

    std::size_t pos = 0;
    while (pos < rest.size()) {
        const std::size_t space = rest.find(' ', pos);
        const std::string_view token = (space == std::string_view::npos) ? rest.substr(pos) : rest.substr(pos, space - pos);
        if (!token.empty()) {
            const auto [key, value] = split_key_value(token, '=');
            if (key == "kind") {
                kind = std::string(value);
                have_kind = true;
            } else if (key == "weight_kg") {
                weight_kg = std::stod(std::string(value));
                have_weight = true;
            } else if (key == "declared_value_cents") {
                declared_value_cents = std::stoll(std::string(value));
                have_value = true;
            }
            // The handling_code token is intentionally ignored here:
            // make_package() derives handling_code from kind, which is
            // always the same value this line's own handling_code token
            // carries, so there is nothing new to parse out of it.
        }
        if (space == std::string_view::npos) {
            break;
        }
        pos = space + 1;
    }

    if (!have_kind || !have_weight || !have_value) {
        throw std::invalid_argument("deserialize_manifest: incomplete PACKAGE line");
    }
    return make_package(kind, weight_kg, declared_value_cents);
}

}  // namespace

std::string serialize_manifest(const ShipmentManifest& manifest_obj) {
    std::string text;
    text += std::format("SHIPMENT_ID={}\n", manifest_obj.shipment_id);
    text += std::format("CARRIER={}\n", manifest_obj.carrier);
    text += std::format("DESTINATION={}\n", manifest_obj.destination);
    text += std::format("PRIORITY={}\n", to_string(manifest_obj.priority));
    text += std::format("INSURED_VALUE_CENTS={}\n", manifest_obj.insured_value_cents);
    text += std::format("NOTES={}\n", manifest_obj.special_handling_notes);
    for (const auto& package : manifest_obj.packages) {
        text += std::format("PACKAGE kind={} weight_kg={:.2f} handling_code={} declared_value_cents={}\n", package->kind(),
                             package->weight_kg(), package->handling_code(), package->declared_value_cents());
    }
    return text;
}

ShipmentManifest deserialize_manifest(const std::string& text) {
    ShipmentManifest result;

    std::size_t line_start = 0;
    while (line_start <= text.size()) {
        const std::size_t line_end = text.find('\n', line_start);
        const std::string_view raw_line =
            (line_end == std::string::npos) ? std::string_view(text).substr(line_start) : std::string_view(text).substr(line_start, line_end - line_start);
        const std::string_view line = trim(raw_line);

        if (!line.empty()) {
            if (line.starts_with("PACKAGE")) {
                result.packages.push_back(parse_package_line(line));
            } else {
                const auto [key, value] = split_key_value(line, '=');
                if (key == "SHIPMENT_ID") {
                    result.shipment_id = std::string(value);
                } else if (key == "CARRIER") {
                    result.carrier = std::string(value);
                } else if (key == "DESTINATION") {
                    result.destination = std::string(value);
                } else if (key == "PRIORITY") {
                    result.priority = priority_from_string(value);
                } else if (key == "INSURED_VALUE_CENTS") {
                    result.insured_value_cents = std::stoll(std::string(value));
                } else if (key == "NOTES") {
                    result.special_handling_notes = std::string(value);
                } else {
                    throw std::invalid_argument("deserialize_manifest: unrecognized header key '" + std::string(key) + "'");
                }
            }
        }

        if (line_end == std::string::npos) {
            break;
        }
        line_start = line_end + 1;
    }

    return result;
}

std::string summary_line(const ShipmentManifest& manifest_obj) {
    return std::format("{} via {} to {} [{}], {} package(s), insured for ${}.{:02d}", manifest_obj.shipment_id, manifest_obj.carrier,
                        manifest_obj.destination, to_string(manifest_obj.priority), manifest_obj.packages.size(),
                        manifest_obj.insured_value_cents / 100, manifest_obj.insured_value_cents % 100);
}

#if __cplusplus >= 202302L
void print_summary(const ShipmentManifest& manifest_obj) {
    std::println("{} via {} to {} [{}], {} package(s), insured for ${}.{:02d}", manifest_obj.shipment_id, manifest_obj.carrier,
                  manifest_obj.destination, to_string(manifest_obj.priority), manifest_obj.packages.size(),
                  manifest_obj.insured_value_cents / 100, manifest_obj.insured_value_cents % 100);
}
#endif

}  // namespace manifest
