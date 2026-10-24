#include "email_gateway.hpp"

namespace notify {

namespace {

// A tiny built-in template table standing in for a real template store.
// "{message}" is replaced with the event's own message body.
std::string template_body(const std::string& template_id) {
    if (template_id == "order-delayed") {
        return "We're sorry, your order is running late. {message}";
    }
    return "{message}";
}

std::string substitute_message(const std::string& body, const OrderEvent& event) {
    const std::string placeholder = "{message}";
    const auto pos = body.find(placeholder);
    if (pos == std::string::npos) {
        return body;
    }
    return body.substr(0, pos) + event.message_body + body.substr(pos + placeholder.size());
}

}  // namespace

DispatchResult EmailGateway::send(const OrderEvent& event) const {
    const std::string dispatch_id = "email-" + event.order_id;
    dispatch_log_[dispatch_id] = DispatchStatus::Sent;
    sent_bodies_[dispatch_id] = format_message(event);
    return DispatchResult{DispatchStatus::Sent, dispatch_id};
}

std::string EmailGateway::render(const std::string& template_id, const OrderEvent& event) const {
    return substitute_message(template_body(template_id), event);
}

DispatchStatus EmailGateway::status_of(const std::string& dispatch_id) const {
    const auto found = dispatch_log_.find(dispatch_id);
    return found == dispatch_log_.end() ? DispatchStatus::Failed : found->second;
}

std::string EmailGateway::body_sent_for(const std::string& dispatch_id) const {
    const auto found = sent_bodies_.find(dispatch_id);
    return found == sent_bodies_.end() ? std::string{} : found->second;
}

}  // namespace notify
