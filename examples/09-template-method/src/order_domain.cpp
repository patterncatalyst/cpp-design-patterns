#include "order_domain.hpp"

namespace orderexport {

int total_quantity(const Order& order) {
    int total = 0;
    for (const OrderLine& line : order.lines) {
        total += line.quantity;
    }
    return total;
}

}  // namespace orderexport
