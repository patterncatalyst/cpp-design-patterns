#include "rate_table_adapter.hpp"

namespace warehouse {

#if __cplusplus >= 202302L

// C++23: std::out_ptr binds a temporary raw-pointer slot to the handle
// itself, writes the handle's result into it, and adopts the pointer into
// the handle when the call returns. The handle never holds a raw pointer
// and a smart pointer at once.
RateTableHandle open_rate_table(const std::string& carrier_code) {
    RateTableHandle handle;
    const int rc = legacy_open_rate_table(carrier_code.c_str(), std::out_ptr(handle));
    if (rc != 0) {
        return RateTableHandle{};
    }
    return handle;
}

#else

// C++20: no std::out_ptr exists yet, so the raw pointer has to be a real,
// named local variable, passed to the legacy function by address, then
// handed to the handle's constructor by hand. The raw pointer is live and
// unmanaged for the lines between the call and the handle construction.
RateTableHandle open_rate_table(const std::string& carrier_code) {
    CRateTable* raw_table = nullptr;
    const int rc = legacy_open_rate_table(carrier_code.c_str(), &raw_table);
    if (rc != 0) {
        return RateTableHandle{};
    }
    return RateTableHandle{raw_table};
}

#endif

}  // namespace warehouse
