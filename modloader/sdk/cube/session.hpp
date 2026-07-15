#pragma once
// Session + HUD accessors: Session, Ui.

#include "cube/common.hpp"

namespace cube
{
    class Session
    {
    public:
        explicit Session(const CubeApi* api) : m_api(api), m_valid(api && api->session.get(api, &m_data) != 0) {}

        bool valid() const { return m_valid; }
        bool refresh() { m_valid = m_api && m_api->session.get(m_api, &m_data) != 0; return m_valid; }
        bool reload() { return refresh(); }
        GameState getState() const { return static_cast<GameState>(m_data.gameState); }
        bool isInWorld() const { return getState() == GameState::InWorld; }
        bool hasNetwork() const { return m_data.hasNetwork != 0; }
        NetworkMode getNetworkMode() const { return static_cast<NetworkMode>(m_data.networkMode); }
        bool isMultiplayer() const { return getNetworkMode() == NetworkMode::Multiplayer; }
        bool isConnected() const { return m_data.connected != 0; }
        int getPlayerCount() const { return m_data.playerCount; }
        unsigned getAddress() const { return m_data.address; } // GameController base (raw)
        const CubeSession& raw() const { return m_data; }
        // Live edits to the network state bytes (SP/MP + connected flag).
        bool setField(SessionField field, int value) const { return m_api && m_api->session.setField(m_api, static_cast<int32_t>(field), value) != 0; }
        bool setMultiplayer(bool multiplayer) const { return setField(SessionField::NetworkMode, multiplayer ? 1 : 0); }
        bool setConnected(bool connected) const { return setField(SessionField::Connected, connected ? 1 : 0); }

    private:
        const CubeApi* m_api;
        CubeSession m_data = {};
        bool m_valid;
    };

    // Which HUD menus are open (so a mod can avoid grabbing input while in a menu).
    class Ui
    {
    public:
        explicit Ui(const CubeApi* api) : m_api(api), m_valid(api && api->ui.get(api, &m_data) != 0) {}

        bool valid() const { return m_valid; }
        bool refresh() { m_valid = m_api && m_api->ui.get(m_api, &m_data) != 0; return m_valid; }
        bool reload() { return refresh(); }
        bool isInventoryOpen() const { return m_data.inventoryOpen != 0; }
        bool isCharacterOpen() const { return m_data.characterOpen != 0; }
        bool isMapOpen() const { return m_data.mapOpen != 0; }
        bool isObjectiveOpen() const { return m_data.objectiveOpen != 0; }
        bool isMenuOpen() const { return m_data.anyOpen != 0; }
        unsigned getAddress() const { return m_data.address; } // GameController base (raw)
        const CubeUi& raw() const { return m_data; }
        // Force HUD panels open/closed. Input-driven panels may re-close next frame.
        bool setField(UiField field, bool open) const { return m_api && m_api->ui.setField(m_api, static_cast<int32_t>(field), open ? 1 : 0) != 0; }
        bool setInventoryOpen(bool open) const { return setField(UiField::Inventory, open); }
        bool setCharacterOpen(bool open) const { return setField(UiField::Character, open); }
        bool setMapOpen(bool open) const { return setField(UiField::Map, open); }
        bool setObjectiveOpen(bool open) const { return setField(UiField::Objective, open); }

    private:
        const CubeApi* m_api;
        CubeUi m_data = {};
        bool m_valid;
    };
}
