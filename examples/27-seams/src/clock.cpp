#include "clock.hpp"

namespace seam27 {

std::chrono::system_clock::time_point SystemClock::now() const {
    return std::chrono::system_clock::now();
}

}  // namespace seam27
