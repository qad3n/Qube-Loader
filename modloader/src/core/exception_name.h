#pragma once
// Win32 structured-exception code to a short display name (shared by crash + faultguard).

#include <windows.h>

namespace core
{
    inline const char* exceptionName(DWORD code)
    {
        constexpr DWORD kStatusStackBufferOverrun = 0xC0000409;
        switch (code)
        {
            case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
            case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
            case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
            case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
            case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
            case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
            case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
            case EXCEPTION_FLT_INVALID_OPERATION: return "FLT_INVALID_OPERATION";
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
            case kStatusStackBufferOverrun: return "STACK_BUFFER_OVERRUN";
            default: return "EXCEPTION";
        }
    }
}
