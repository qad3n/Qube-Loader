#include "game/assets.h"
#include "game/assetcodec.h"
#include "game/signature.h"
#include "game/offsets.h"
#include "hooks/detour.h"
#include "core/mem.h"
#include "core/log.h"
#include "util/fmt.h"
#include "util/guard.h"
#include "util/inflight.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace game::assets
{
    namespace
    {
        constexpr char kCategory[] = "assets";
        constexpr char kRawExtension[] = ".cub"; // data1 is stored un-obfuscated; all other DBs re-encode
        constexpr uint32_t kMaxKeyLength = 260;  // filename key upper bound (bounds the guarded string read)
        constexpr int32_t kMaxBlobBytes = 64 * 1024 * 1024; // sanity cap on a single override blob
        constexpr uint8_t kSelfTestLen = 64;     // codec round-trip sample size
        constexpr uint8_t kSelfTestStep = 7;     // arbitrary non-trivial byte pattern for the self-test

        // db::loadBlobByKey is __thiscall(Database* this, std::string* key, void** outBlob, size_t*
        // outSize). On mingw a __thiscall with stack args is ABI-identical to __fastcall(self, edx, ...):
        // ECX -> self, EDX is an unused dummy, and the three stack args follow.
        typedef uint32_t(__fastcall* LoadBlobFn)(void* self, void* edx, void* key, void** outBlob, uint32_t* outSize);
        typedef void*(__cdecl* OperatorNewFn)(uint32_t size);

        std::mutex g_mutex;
        std::map<std::string, std::vector<uint8_t>> g_overrides; // key (filename) -> plaintext bytes
        int32_t g_key[off::kBlobShuffleKeyCount] = {};           // shuffle key, loaded from .rdata at install

        LoadBlobFn g_original = nullptr;
        std::atomic<bool> g_active{false};
        std::atomic<int> g_inFlight{0};
        bool g_installed = false;

        void* target()
        {
            return reinterpret_cast<void*>(mem::rebase(off::kDbLoadBlobByKey));
        }

        // ASCII-lowercase a byte (letters only; '.' and digits pass through), so the extension test is
        // case-insensitive without pulling in locale-sensitive std::tolower.
        char asciiLower(char c)
        {
            return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
        }

        bool endsWithRawExtension(const std::string& key)
        {
            const std::size_t extLen = sizeof(kRawExtension) - 1;
            if (key.size() < extLen)
                return false;
            const std::size_t base = key.size() - extLen;
            for (std::size_t i = 0; i < extLen; ++i)
            {
                if (asciiLower(key[base + i]) != kRawExtension[i])
                    return false;
            }
            return true;
        }

        // Read the game's MSVC std::string key at obj into out (guarded; never faults). SSO holds the
        // chars inline at +0 while capacity < 16, else +0 is a heap pointer.
        bool readGameString(uintptr_t obj, std::string& out)
        {
            uint32_t size = 0;
            uint32_t cap = 0;
            if (!mem::read(obj + off::kStdStringSizeOff, size) || !mem::read(obj + off::kStdStringCapOff, cap))
                return false;
            if (size == 0 || size > kMaxKeyLength)
                return false;

            uintptr_t dataPtr = obj;
            if (cap >= off::kStdStringSsoCap)
            {
                uint32_t heap = 0;
                if (!mem::read(obj, heap) || !heap)
                    return false;
                dataPtr = heap;
            }

            char buf[kMaxKeyLength] = {};
            if (!mem::readBytes(dataPtr, buf, size))
                return false;
            out.assign(buf, size);
            return true;
        }

        // On an override hit, allocate the blob with the game's operator new (so the caller frees it with
        // operator delete), fill it with the mod's bytes (re-encoded for non-.cub), and report it through
        // the caller's out-args. Returns false to fall through to the original read.
        bool tryOverride(void* keyStr, void** outBlob, uint32_t* outSize, uint32_t& result)
        {
            std::string key;
            if (!readGameString(reinterpret_cast<uintptr_t>(keyStr), key))
                return false;

            std::vector<uint8_t> bytes;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                const std::map<std::string, std::vector<uint8_t>>::const_iterator it = g_overrides.find(key);
                if (it == g_overrides.end())
                    return false;
                bytes = it->second; // copy out so the re-encode runs without holding the lock
            }

            if (!endsWithRawExtension(key))
                assetcodec::encode(bytes, g_key, off::kBlobShuffleKeyCount);

            const OperatorNewFn operatorNew = reinterpret_cast<OperatorNewFn>(mem::rebase(off::kOperatorNew));
            void* buffer = operatorNew(static_cast<uint32_t>(bytes.size()));
            if (!buffer)
                return false;
            if (!bytes.empty())
                std::memcpy(buffer, bytes.data(), bytes.size());

            *outBlob = buffer;
            *outSize = static_cast<uint32_t>(bytes.size());
            result = 1;
            LOGC(Debug, kCategory, "override '%s' -> %zu bytes", key.c_str(), bytes.size());
            return true;
        }

        uint32_t __fastcall detour(void* self, void* edx, void* keyStr, void** outBlob, uint32_t* outSize)
        {
            barrier::InFlight inflight(g_inFlight);
            if (!g_original)
                return 0;

            if (g_active.load(std::memory_order_acquire) && outBlob && outSize)
            {
                uint32_t result = 0;
                bool handled = false;
                guard::tryRun("asset override", [&]()
                {
                    handled = tryOverride(keyStr, outBlob, outSize, result);
                });
                if (handled)
                    return result;
            }
            return g_original(self, edx, keyStr, outBlob, outSize);
        }

        bool loadKeyTable()
        {
            return mem::readBytes(mem::rebase(off::kBlobShuffleKey), g_key, sizeof(g_key));
        }

        // Prove encode() and decode() are mutual inverses under the loaded key, so a corrupt key read is
        // caught before any override goes live. Does not prove parity with the game's decode (that is the
        // standing live-verify item), but catches the most likely failure.
        bool selfTest()
        {
            std::vector<uint8_t> sample;
            sample.reserve(kSelfTestLen);
            for (uint8_t i = 0; i < kSelfTestLen; ++i)
                sample.push_back(static_cast<uint8_t>(i * kSelfTestStep + 1));

            std::vector<uint8_t> work = sample;
            assetcodec::encode(work, g_key, off::kBlobShuffleKeyCount);
            assetcodec::decode(work, g_key, off::kBlobShuffleKeyCount);
            return work == sample;
        }
    }

    bool install()
    {
        if (g_installed)
            return true;
        if (!signature::compatibleBuild())
        {
            LOGC(Warn, kCategory, "asset injection disabled (Cube.exe build mismatch)");
            return false;
        }
        if (!loadKeyTable() || !selfTest())
        {
            LOGC(Warn, kCategory, "asset injection disabled (codec key @0x%08X unavailable or self-test failed)",
                 fmt::u32(off::kBlobShuffleKey));
            return false;
        }
        if (!hooks::detour::create(target(), reinterpret_cast<void*>(&detour), reinterpret_cast<void**>(&g_original)))
        {
            LOGC(Warn, kCategory, "asset injection detour failed to install");
            return false;
        }

        g_active.store(true, std::memory_order_release);
        g_installed = true;
        LOGC(Debug, kCategory, "asset injection installed (loadBlobByKey detour active)");
        return true;
    }

    void remove()
    {
        if (!g_installed)
            return;

        g_active.store(false, std::memory_order_release); // flip to pass-through first
        barrier::drain(g_inFlight, kCategory);             // drain in-flight calls before unpatching
        hooks::detour::remove(target());
        g_original = nullptr;
        g_installed = false;

        std::lock_guard<std::mutex> lock(g_mutex);
        g_overrides.clear();
    }

    bool available()
    {
        return signature::compatibleBuild();
    }

    bool setOverride(const std::string& key, const void* data, int32_t size)
    {
        if (key.empty() || !data || size <= 0 || size > kMaxBlobBytes)
            return false;
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        std::lock_guard<std::mutex> lock(g_mutex);
        g_overrides[key].assign(bytes, bytes + size);
        return true;
    }

    bool removeOverride(const std::string& key)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_overrides.erase(key) != 0;
    }
}
