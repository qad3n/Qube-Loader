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

    // Scans held, equipped and stored items for corruption and warns on each new one. Throttled,
    // duplicate suppressed, and a defect must appear in two scans before it warns.
    void pollItemCorruption();
}
