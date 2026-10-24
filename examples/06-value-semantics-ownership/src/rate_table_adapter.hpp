#pragma once

#include <memory>
#include <string>

#include "legacy_rate_table.h"

namespace warehouse {

// legacy_close_rate_table() is the deleter a unique_ptr needs to adopt a
// CRateTable* safely: this functor is the only place in this example that
// still calls it directly.
struct RateTableDeleter {
    void operator()(CRateTable* table) const noexcept {
        legacy_close_rate_table(table);
    }
};

using RateTableHandle = std::unique_ptr<CRateTable, RateTableDeleter>;

// Opens carrier_code against the legacy rate service and returns an empty
// handle if the carrier is unrecognized. Everywhere past this function,
// the raw CRateTable* from legacy_open_rate_table() is gone; callers hold
// a RateTableHandle and never see the pointer legacy_close_rate_table()
// expects back.
RateTableHandle open_rate_table(const std::string& carrier_code);

}  // namespace warehouse
