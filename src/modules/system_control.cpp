#include "modules/system_control.hpp"
#include <iostream>

bool SystemControl::shutdown() {
    std::cout << "[SystemControl] Stub shutdown()\n";
    return false;
}

bool SystemControl::restart() {
    std::cout << "[SystemControl] Stub restart()\n";
    return false;
}
