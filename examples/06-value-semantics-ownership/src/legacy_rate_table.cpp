#include "legacy_rate_table.h"

#include <cstdlib>
#include <cstring>

// Implemented in C++ but compiled to the plain C ABI the header declares
// (extern "C", malloc/free instead of new/delete) so the rest of this
// example can treat it exactly like a vendor C library it links against:
// a factory function that hands back a raw pointer, and a matching free
// function, with no C++ ownership type anywhere in the interface.
int legacy_open_rate_table(const char* carrier_code, CRateTable** out_table) {
    double rate_cents;

    if (std::strcmp(carrier_code, "northwind") == 0) {
        rate_cents = 285.0;
    } else if (std::strcmp(carrier_code, "cardinal") == 0) {
        rate_cents = 310.0;
    } else {
        return -1;
    }

    CRateTable* table = static_cast<CRateTable*>(std::malloc(sizeof(CRateTable)));
    if (table == nullptr) {
        return -1;
    }
    table->per_kg_rate_cents = rate_cents;
    *out_table = table;
    return 0;
}

void legacy_close_rate_table(CRateTable* table) {
    std::free(table);
}
