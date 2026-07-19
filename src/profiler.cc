#include "profiler.h"

#if ENABLE_PROFILING
#include "debug.h"
static_assert(ENABLE_DEBUGGING, "Need debugging for logging");

namespace engine::profiler {

namespace {

internal::ProfileStorage * s_active;

} // namespace

internal::ProfileStorage::ProfileStorage(const char *n) : name(n) {
    // Add us to the list.
    next = s_active;
    s_active = this;
}

void print_timings() {
    for (auto * info = s_active; info != nullptr; info = info->next) {
        // Read the current scope's total and reset.
        auto total = info->total;
        info->total = 0;

        // Log it if it came up.
        if (total > 0) {
            DEBUG_MSG("P:", info->name, ",", total);
        }
    }
}

} // namespace engine::profiler

#endif
