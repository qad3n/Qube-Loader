#pragma once
// Rolling history of the local player's health, sampled once per frame by the mod's onFrame. The
// Combat tab plots data()/head() as a pure view; the sampling loop lives here, not in the menu.

namespace exmod
{

    class HealthHistory
    {
    public:
        static constexpr int kSize = 120;

        void sample();
        const float* data() const { return m_health; }
        int size() const { return kSize; }
        int head() const { return m_head; }

    private:
        float m_health[kSize] = {};
        int m_head = 0;
    };

    HealthHistory& healthHistory();

}
