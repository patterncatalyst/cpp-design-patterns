#pragma once

#include <string>

namespace warehouse {

// A parcel waiting to move through a regional dispatch warehouse. Every
// member is a plain value (std::string, double) with its own correct copy
// and move behavior, so Parcel needs none of the Rule of Five: no
// destructor, no copy or move constructor, no copy or move assignment.
// The compiler-generated special members are already correct, which is
// the Rule of Zero this chapter trains: write a resource-owning type only
// when a type actually owns a resource the compiler doesn't already know
// how to copy.
struct Parcel {
    std::string tracking_id;
    double weight_kg;
    std::string label;
};

}  // namespace warehouse
