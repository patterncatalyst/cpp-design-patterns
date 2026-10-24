#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace shipping::audit {

// A thread-safe Meyers singleton: the one and only ShipmentAuditLog for the
// whole process, reached through instance() from anywhere, by anything,
// with no constructor parameter announcing the dependency to a caller.
//
// The function-local static inside instance() is safely constructed exactly
// once even under concurrent first calls, a guarantee C++11 onward gives
// every function-local static with no extra code required. That guarantee
// covers construction only; everything after construction, every record()
// and entries() call, still needs its own synchronization, hence mutex_
// below.
class ShipmentAuditLog {
public:
    static ShipmentAuditLog& instance();

    ShipmentAuditLog(const ShipmentAuditLog&) = delete;
    ShipmentAuditLog& operator=(const ShipmentAuditLog&) = delete;

    void record(std::string event);
    std::vector<std::string> entries() const;

    // Exists only so this chapter's own tests can isolate themselves from
    // each other. Its presence is itself evidence of the design's cost: a
    // type with no global state would never need a method whose only job
    // is to pretend the global state was never touched.
    void reset_for_testing();

private:
    ShipmentAuditLog() = default;

    mutable std::mutex mutex_;
    std::vector<std::string> entries_;
};

}  // namespace shipping::audit
