#pragma once

namespace seam27 {

// Inherited billing routine: converts how many minutes a shipment ran past
// its SLA deadline into a late fee, in cents, once the enterprise grace
// period has elapsed. This is a bare free function called from dozens of
// sites in the billing batch job, with no constructor anywhere nearby and
// no object a test could construct with an injected collaborator; that is
// what rules out an object seam for the grace_period() dependency it
// calls internally and makes it the motivating case for a link seam
// instead.
int late_fee_cents(int minutes_late);

}  // namespace seam27
