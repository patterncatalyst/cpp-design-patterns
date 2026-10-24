#pragma once

#include <string>

#include "manifest_domain.hpp"

namespace manifest {

// Serializes a manifest to a simple line-based text format: one
// KEY=VALUE header line per scalar field, then one PACKAGE line per
// package, each a space-separated list of key=value tokens. The format is
// deliberately plain so a diff between two manifests stays readable; it is
// a teaching format for this chapter's round trip, not a real EDI dialect.
std::string serialize_manifest(const ShipmentManifest& manifest_obj);

// Parses the format serialize_manifest() produces, reconstructing each
// package through make_package() (package_prototype.hpp) rather than
// through the PackageTemplateRegistry, since a deserialized package
// carries its own serialized weight and declared value, not a template's
// defaults. Throws std::invalid_argument on a line this parser does not
// recognize.
ShipmentManifest deserialize_manifest(const std::string& text);

// A one-line human-readable summary, built with std::format. Compiles
// under both C++20 and C++23.
std::string summary_line(const ShipmentManifest& manifest_obj);

#if __cplusplus >= 202302L
// C++23 refinement: write the same summary straight to stdout with
// std::print, with no intermediate std::format buffer and no std::cout
// formatting stream at the call site.
void print_summary(const ShipmentManifest& manifest_obj);
#endif

}  // namespace manifest
