#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* A deliberately C-style legacy API: a flat per-kilogram rate table a
 * carrier's older on-premises pricing service hands out through a
 * malloc-backed factory function, the same shape a real codebase finds
 * wrapping a vendor C library or a system call. This header has no C++
 * types in it on purpose, so wrapping it below is a genuine example of
 * adopting a raw pointer from a C-style factory into a smart pointer, not
 * a C++ type dressed up as C. */

typedef struct CRateTable {
    double per_kg_rate_cents;
} CRateTable;

/* Returns 0 and writes a newly allocated CRateTable* through *out_table
 * when carrier_code is recognized. Returns -1 and leaves *out_table
 * untouched for an unknown carrier code. */
int legacy_open_rate_table(const char* carrier_code, CRateTable** out_table);

/* Frees a table returned by legacy_open_rate_table. Passing NULL is safe
 * and does nothing, matching free()'s contract. */
void legacy_close_rate_table(CRateTable* table);

#ifdef __cplusplus
}
#endif
