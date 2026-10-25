#pragma once

// Form one: an enum-indexed function-pointer table, the classic jump
// table. g_jump_table is a plain array, one slot per CarrierMessageType, in
// the enum's declaration order. Nothing in the type system ties that order
// to the enum; it is a convention a reviewer checks by eye every time the
// enum changes. See dispatch_table_constexpr.hpp for the form that removes
// that convention.

#include "message_handlers.hpp"

namespace dispatch25 {

extern MessageHandler g_jump_table[kCarrierMessageTypeCount];

void dispatch_via_jump_table(const CarrierMessage& message, TrackingSink& sink);

}  // namespace dispatch25
