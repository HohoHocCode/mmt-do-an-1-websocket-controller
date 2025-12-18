#include <iostream>

namespace stub {
void modules_unavailable_notice();
void network_unavailable_notice();
}

int main() {
    stub::modules_unavailable_notice();
    stub::network_unavailable_notice();
    std::cerr << "[stub] server built without Boost/OpenCV; runtime features are not available.\n";
    return 0;
}
