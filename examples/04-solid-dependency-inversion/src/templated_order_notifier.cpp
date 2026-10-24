#include "templated_order_notifier.hpp"

namespace notify {

DispatchResult TemplatedOrderNotifier::notify_customer(const OrderEvent& event) const {
    OrderEvent rendered_event = event;
    rendered_event.message_body = renderer_.render(template_id_, event);
    return sender_.send(rendered_event);
}

}  // namespace notify
