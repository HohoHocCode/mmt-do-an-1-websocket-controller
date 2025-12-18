#include <iostream>

namespace stub {
void modules_unavailable_notice() {
    static bool shown = false;
    if (shown) {
        return;
    }
    std::cerr << "[stub] modules unavailable: Boost/OpenCV not found; functionality disabled.\n";
    shown = true;
}
} // namespace stub
