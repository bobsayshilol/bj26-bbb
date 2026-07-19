#pragma once

//
// Profiling helpers.
//
// Usage:
//
//   PROFILE_STORAGE(example);
//
//   void do_thing() {
//     PROFILE_SCOPE(example);
//   }
//
//   void update() {
//     do_thing();
//     engine::profiler::print_timings();
//   }
//

#include "utils.h"

// WARNING: this won't work on a real device because of read_timer()
#define ENABLE_PROFILING 1

namespace engine::profiler {

#if ENABLE_PROFILING

void print_timings();

#define PROFILE_STORAGE(name) static engine::profiler::internal::ProfileStorage _prof_storage_##name (#name)

#define PROFILE_SCOPE(name) engine::profiler::internal::ScopedProfile CONCAT(_prof_scope_, __LINE__) (_prof_storage_##name)

namespace internal {

inline uint32_t read_timer() {
    // HACK: a proper way of doing this would be to read the timer with an ISR,
    // but I'm lazy so I've added a custom instruction to LoopyMSE instead.
    uint32_t t = 0;
    __asm__ volatile (
        ".word 0x0001 \n"   // fake rdtsc-like instruction
        "mov r0, %0"        // move it into result because GCC doesn't like "=r0" (TODO: is "=a" r0?)
        : "=r"(t)           // output result to |t|
        :                   // no inputs
        : "r0"              // clobbers r0
    );
    return t;
}

struct ProfileStorage {
    ProfileStorage(const char *n);
    uint32_t total; // should be fine if called per-frame
    ProfileStorage *next;
    const char *name;
};

struct ScopedProfile {
    ProfileStorage & storage;
    ScopedProfile(ProfileStorage & st) : storage(st) { st.total -= read_timer(); }
    ~ScopedProfile() { storage.total += read_timer(); }
};

} // namespace internal

#else

inline void print_timings() { }

#define PROFILE_STORAGE(name)

#define PROFILE_SCOPE(name)

#endif

} // engine::profiler
