#pragma once
// Continuous per frame cheats. The Hero tab binds widgets to settings(); the mod's onFrame calls
// apply() every frame regardless of menu visibility. All behavior lives here, none in the menu.

namespace exmod
{

    constexpr float kDefaultGodHealth = 100.0f;

    class Cheats
    {
    public:
        struct Settings
        {
            bool godMode = false;
            float godModeHealth = kDefaultGodHealth;
            bool infiniteMana = false;
            bool infiniteStamina = false;
        };

        Settings& settings() { return m_settings; }
        void apply() const;

    private:
        Settings m_settings;
    };

    Cheats& cheats();

}
