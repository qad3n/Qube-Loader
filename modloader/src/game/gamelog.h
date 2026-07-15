#pragma once
// Captures game output (OutputDebugStringA/W, MessageBoxA, SQLite xLog) into our log.

namespace gamelog
{
    bool install();
    void remove();
}