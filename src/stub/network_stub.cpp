#include <iostream>

namespace stub {
void network_unavailable_notice() {
    static bool shown = false;
    if (shown) {
        return;
    }
    std::cerr << "[stub] network unavailable: Boost/OpenCV not found; websocket features disabled.\n";
    shown = true;
}
} // namespace stub
