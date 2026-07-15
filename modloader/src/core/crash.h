#pragma once
// Logs crash fault address (rebased for attribution.tsv), registers, stack, minidump.
#include <string>

namespace crash
{
    void install(const std::string& dumpDir);
    void remove();
}
