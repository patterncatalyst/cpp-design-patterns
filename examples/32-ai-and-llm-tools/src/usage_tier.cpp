#include "usage_tier.hpp"

const char* ToString(UsageTier tier) {
    switch (tier) {
        case UsageTier::Starter:
            return "Starter";
        case UsageTier::Growth:
            return "Growth";
        case UsageTier::Scale:
            return "Scale";
    }
    return "Unknown";
}
