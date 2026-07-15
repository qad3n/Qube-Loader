#pragma once
// One loud channel for every multi-mod conflict warning. All contention messages carry a CONFLICT:
// prefix under a single category so they are unmissable in the console and the run.sh tail (Warn is
// yellow, Error is red). warn() = contention that resolves last-writer-wins; error() = duplicate
// identity, the most dangerous case.

#include <cstdint>

namespace modloader::conflict
{
    void warn(const char* fmt, ...);
    void error(const char* fmt, ...);

    // Rate-limit repeated warnings about the same conflict. Returns true at most once per
    // cooldownFrames for a given signature; shared by the write and hook conflict detectors.
    bool throttle(uint64_t signature, uint32_t frame, uint32_t cooldownFrames);
}
