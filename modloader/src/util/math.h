#pragma once
// Small vector helpers, kept out of game logic.

#include <cmath>

namespace mathutil
{

    struct Vec3
    {
        float x;
        float y;
        float z;
    };

    inline float distance3(const Vec3& a, const Vec3& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;

        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    inline float magnitude2(float x, float y)
    {
        return std::sqrt(x * x + y * y);
    }

    constexpr float kPi = 3.14159265f;
    constexpr float kTwoPi = 6.28318531f;
    constexpr float kDegToRad = 0.017453292f; // pi/180

    // Normalize an angle in radians to [-pi, pi). The game stores some yaw/phase angles unwrapped
    // (grow without bound), so callers want a bounded, displayable value.
    inline float wrapRadians(float radians)
    {
        float wrapped = std::fmod(radians + kPi, kTwoPi);
        if (wrapped < 0.0f)
            wrapped += kTwoPi;

        return wrapped - kPi;
    }
}