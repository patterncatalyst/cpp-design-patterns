#pragma once

// Extract Interface: AlertChannel is the minimal surface
// ShipmentAlertNotifier actually needs from its mail dependency. The
// inherited code called a concrete SmtpAlertSender directly, which in a
// real warehouse system also exposes connection pooling, retry
// configuration, and TLS options nothing here touches. Pulling out just
// the one method a caller uses turns "the only implementation I can ever
// construct" into "anything that can send a message to a recipient."

#include <string>

namespace breakdep28 {

class AlertChannel {
public:
    virtual ~AlertChannel() = default;
    virtual void send(const std::string& recipient, const std::string& message) const = 0;
};

// The legacy concrete type ShipmentAlertNotifier used to construct and
// call directly, with no interface between them. Stands in for a real
// SMTP client: it writes a line to stdout instead of opening a socket, so
// this chapter's example and its tests run with no network dependency.
class SmtpAlertSender : public AlertChannel {
public:
    explicit SmtpAlertSender(std::string smtp_host);

    void send(const std::string& recipient, const std::string& message) const override;

private:
    std::string smtp_host_;
};

}  // namespace breakdep28
