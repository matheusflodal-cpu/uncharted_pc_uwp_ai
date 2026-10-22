#include "boot_trace.h"
void trace_from_peer() {
    UnchartedBootTrace::Scope scope(UnchartedBootTrace::Kind::Compute,0xabc);
}
