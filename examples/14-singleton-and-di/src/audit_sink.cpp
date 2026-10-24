#include "audit_sink.hpp"

namespace shipping::audit {

bool InMemoryAuditSink::record(const std::string& event) {
    entries_.push_back(event);
    return true;
}

std::vector<std::string> InMemoryAuditSink::entries() const {
    return entries_;
}

}  // namespace shipping::audit
