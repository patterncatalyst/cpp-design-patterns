#include "alert_channel.hpp"

#include <iostream>

namespace breakdep28 {

SmtpAlertSender::SmtpAlertSender(std::string smtp_host) : smtp_host_(std::move(smtp_host)) {}

void SmtpAlertSender::send(const std::string& recipient, const std::string& message) const {
    std::cout << "  [smtp:" << smtp_host_ << "] to=" << recipient << " \"" << message << "\"\n";
}

}  // namespace breakdep28
