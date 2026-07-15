// LoadLibrary injector: inject.exe <process.exe> <path\to\cube_mod.dll>
#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cstring>

namespace
{

    constexpr char kKernel32[] = "kernel32.dll";
    constexpr char kLoadLibraryProc[] = "LoadLibraryA";
    constexpr int kExpectedArgs = 3;
    constexpr DWORD kInjectAccess = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;

    DWORD findPid(const char* imageName)
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE)
            return 0;

        PROCESSENTRY32 pe{};
        pe.dwSize = sizeof(pe);
        DWORD pid = 0;
        if (Process32First(snap, &pe))
        {
            do
            {
                if (_stricmp(pe.szExeFile, imageName) == 0)
                {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
        return pid;
    }

}

int main(int argc, char** argv)
{
    if (argc != kExpectedArgs)
    {
        fprintf(stderr, "usage: %s <process.exe> <full\\path\\to.dll>\n", argv[0]);
        return 1;
    }

    const char* image = argv[1];
    const char* dll = argv[2];

    char full[MAX_PATH];
    const DWORD fullLen = GetFullPathNameA(dll, MAX_PATH, full, nullptr);
    if (fullLen == 0 || fullLen >= MAX_PATH)
    {
        fprintf(stderr, "cannot resolve dll path: %s\n", dll);
        return 1;
    }
    if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES)
    {
        fprintf(stderr, "dll not found: %s\n", full);
        return 1;
    }

    const DWORD pid = findPid(image);
    if (!pid)
    {
        fprintf(stderr, "process not running: %s\n", image);
        return 1;
    }

    HANDLE proc = OpenProcess(kInjectAccess, FALSE, pid);
    if (!proc)
    {
        fprintf(stderr, "OpenProcess failed (%lu)\n", GetLastError());
        return 1;
    }

    const size_t len = strlen(full) + 1;
    void* remote = VirtualAllocEx(proc, nullptr, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote)
    {
        fprintf(stderr, "VirtualAllocEx failed\n");
        CloseHandle(proc);
        return 1;
    }

    SIZE_T written = 0;
    if (!WriteProcessMemory(proc, remote, full, len, &written) || written != len)
    {
        fprintf(stderr, "WriteProcessMemory failed (%lu)\n", GetLastError());
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);
        return 1;
    }

    // LoadLibraryA's signature differs from LPTHREAD_START_ROUTINE but is ABI-compatible for
    // CreateRemoteThread; cast through void* so -Wcast-function-type does not flag the intended idiom.
    LPTHREAD_START_ROUTINE loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA(kKernel32), kLoadLibraryProc)));
    if (!loadLibrary)
    {
        fprintf(stderr, "cannot resolve %s\n", kLoadLibraryProc);
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);
        return 1;
    }
    HANDLE thread = CreateRemoteThread(proc, nullptr, 0, loadLibrary, remote, 0, nullptr);
    if (!thread)
    {
        fprintf(stderr, "CreateRemoteThread failed (%lu)\n", GetLastError());
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);
        return 1;
    }

    WaitForSingleObject(thread, INFINITE);
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(thread);
    CloseHandle(proc);
    printf("injected %s into %s (pid %lu)\n", full, image, pid);
    return 0;
}
