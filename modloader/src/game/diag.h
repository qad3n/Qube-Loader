#pragma once
// Resolution diagnostics: resolves every game chain and logs each field's value or reason.

namespace game::diag
{
    // One-shot report of every category. Safe any time (title screen reports unavailable).
    void logResolutionReport();

    // Logs intentionally deferred/absent fields, so they are not read as resolution failures.
    void logKnownGaps();

    // Render-thread poll: emits the full report the first frame the player resolves; re-arms on leave.
    void pollResolutionReport();

    // Render-thread poll: periodically scans items for corruption (unrenderable "?" items
    // that can crash on menu-open) and WARNs on each new one. Throttled + de-duplicated.
    void pollItemCorruption();
}
