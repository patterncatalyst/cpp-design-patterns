#pragma once

#include <string>
#include <vector>

namespace shipping::audit {

// The same capability ShipmentAuditLog provides, modeled instead as an
// abstraction a high-level policy can depend on without knowing, or caring,
// which concrete sink backs it. record() returns false when the write
// itself failed (a disk-full backing store, a timed-out network logger),
// which this chapter's injected version can simulate with a test double; a
// hardwired singleton with one compiled-in implementation has no seam to
// simulate that failure at all.
class AuditSink {
public:
    virtual ~AuditSink() = default;
    virtual bool record(const std::string& event) = 0;
};

// The concrete, in-process sink a running program wires in. Equivalent in
// behavior to ShipmentAuditLog, but owned by whichever composition root
// constructs it rather than reached through a global accessor.
class InMemoryAuditSink : public AuditSink {
public:
    bool record(const std::string& event) override;
    std::vector<std::string> entries() const;

private:
    std::vector<std::string> entries_;
};

}  // namespace shipping::audit
