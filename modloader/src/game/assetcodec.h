#pragma once
// The data*.db blob obfuscation codec, recovered from the game's blob_deobfuscate (0x004496a0). The
// game stores encoded blobs and, on read, DECODES them by a keyed reverse-shuffle followed by a
// byte-complement. To inject an asset we must ENCODE our plaintext into that stored form so the game's
// own decode reconstructs it: complement first, then the forward keyed shuffle (the exact inverse of
// the decode's reverse shuffle). Both passes are length-preserving. The key is a 44-entry int32 table
// read live from the image (.rdata @0x006ffa68). decode() is kept only for the install-time round-trip
// self-test (decode(encode(x)) == x), which catches a bad key load before any override goes live.

#include <cstdint>
#include <vector>

namespace game::assetcodec
{
    // Index map shared by both passes: j(i) = (uint32)(key[i % keyCount] + i) mod n. Data-independent,
    // so a swap sequence and its reverse are exact inverses.
    inline std::size_t shuffleIndex(std::size_t i, const int32_t* key, int32_t keyCount, std::size_t n)
    {
        const uint32_t k = static_cast<uint32_t>(key[i % static_cast<std::size_t>(keyCount)] + static_cast<int32_t>(i));
        return static_cast<std::size_t>(k % static_cast<uint32_t>(n));
    }

    // Encode plaintext into the game's stored form. Inverse of decode(): complement, then swap forward
    // i = 0..n-1 (decode swaps backward, so forward is its inverse).
    inline void encode(std::vector<uint8_t>& bytes, const int32_t* key, int32_t keyCount)
    {
        const std::size_t n = bytes.size();
        if (n == 0 || keyCount <= 0 || !key)
            return;
        for (std::size_t i = 0; i < n; ++i)
            bytes[i] = static_cast<uint8_t>(~bytes[i]);
        for (std::size_t i = 0; i < n; ++i)
        {
            const std::size_t j = shuffleIndex(i, key, keyCount, n);
            const uint8_t tmp = bytes[i];
            bytes[i] = bytes[j];
            bytes[j] = tmp;
        }
    }

    // Decode stored bytes to plaintext, mirroring the game's blob_deobfuscate: reverse-shuffle
    // (i = n-1..0), then complement. Used only by the self-test.
    inline void decode(std::vector<uint8_t>& bytes, const int32_t* key, int32_t keyCount)
    {
        const std::size_t n = bytes.size();
        if (n == 0 || keyCount <= 0 || !key)
            return;
        for (int32_t i = static_cast<int32_t>(n) - 1; i >= 0; --i)
        {
            const std::size_t j = shuffleIndex(static_cast<std::size_t>(i), key, keyCount, n);
            const uint8_t tmp = bytes[static_cast<std::size_t>(i)];
            bytes[static_cast<std::size_t>(i)] = bytes[j];
            bytes[j] = tmp;
        }
        for (std::size_t i = 0; i < n; ++i)
            bytes[i] = static_cast<uint8_t>(~bytes[i]);
    }
}
