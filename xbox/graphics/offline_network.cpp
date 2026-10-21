// No network guest imports are registered by the experimental offline renderer.
#include <cstdio>
extern "C" void bbnet_register() {
    std::puts("UWP offline renderer: network and PSN guest imports remain unresolved");
}
