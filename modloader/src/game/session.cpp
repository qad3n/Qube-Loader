#include "game/session.h"
#include "game/gamecontroller.h"
#include "game/entities.h"
#include "game/offsets.h"
#include "core/mem.h"

#include <cstdint>

namespace game
{
    namespace
    {
        // Resolves a HUD widget's visibility cell (&array[index]). Guarded, so a bad chain fails cleanly.
        bool resolveWidgetCell(uintptr_t gc, uintptr_t slotOff, uintptr_t& cellAddr)
        {
            uint32_t widget = 0;
            if (!mem::read(gc + slotOff, widget) || !widget)
                return false;

            uint32_t state = 0;
            if (!mem::read(widget + off::kWidgetStateOff, state) || !state)
                return false;

            int32_t index = 0;
            uint32_t array = 0;

            if (!mem::read(state + off::kWidgetIndexOff, index) || index < 0 || index >= off::kWidgetMaxIndex || !mem::read(state + off::kWidgetArrayOff, array) || !array)
                return false;

            cellAddr = array + static_cast<uintptr_t>(index) * sizeof(uint32_t);
            return true;
        }

        // array[index] != 0 means the widget is visible.
        bool widgetOpen(uintptr_t gc, uintptr_t slotOff)
        {
            uintptr_t cellAddr = 0;
            uint32_t visible = 0;

            if (!resolveWidgetCell(gc, slotOff, cellAddr) || !mem::read(cellAddr, visible))
                return false;

            return visible != 0;
        }

        bool setWidgetOpen(uintptr_t gc, uintptr_t slotOff, uint8_t open)
        {
            uintptr_t cellAddr = 0;
            if (!resolveWidgetCell(gc, slotOff, cellAddr))
                return false;

            return mem::write<uint32_t>(cellAddr, open);
        }

    }

    bool readSession(CubeSession& out)
    {
        uintptr_t gc = 0;
        uintptr_t creature = 0;

        const bool inWorld = resolveLocalPlayer(gc, creature);
        if (!inWorld && !resolveGameController(gc))
            return false;

        out.address = static_cast<uint32_t>(gc);
        out.gameState = inWorld ? CUBE_STATE_IN_WORLD : CUBE_STATE_TITLE;

        uint8_t mode = 0;
        uint8_t connected = 0;
        if (mem::read(gc + off::kNetModeOff, mode) && mem::read(gc + off::kConnectedOff, connected))
        {
            out.networkMode = mode ? CUBE_NET_MULTIPLAYER : CUBE_NET_SINGLEPLAYER;
            out.connected = connected ? 1 : 0;
            out.hasNetwork = 1;
        }

        out.playerCount = inWorld ? countPlayers(gc) : 0;
        return true;
    }

    bool readUi(CubeUi& out)
    {
        uintptr_t gc = 0;
        if (!resolveGameController(gc))
            return false;

        out.address = static_cast<uint32_t>(gc);
        out.inventoryOpen = widgetOpen(gc, off::kWidgetInventoryOff) ? 1 : 0;
        out.characterOpen = widgetOpen(gc, off::kWidgetCharacterOff) ? 1 : 0;
        out.mapOpen = widgetOpen(gc, off::kWidgetMapOff) ? 1 : 0;

        uint8_t objective = 0;
        if (mem::read(gc + off::kObjectiveOpenByteOff, objective))
            out.objectiveOpen = objective ? 1 : 0;

        out.anyOpen = (out.inventoryOpen || out.characterOpen || out.mapOpen || out.objectiveOpen) ? 1 : 0;
        out.valid = 1;
        return true;
    }

    bool setSessionField(int32_t fieldId, int32_t value)
    {
        uintptr_t gc = 0;
        uintptr_t creature = 0;
        if (!resolveLocalPlayer(gc, creature) && !resolveGameController(gc))
            return false;

        switch (fieldId)
        {
            case CUBE_SESSION_NETWORK_MODE:
                return mem::write<uint8_t>(gc + off::kNetModeOff, static_cast<uint8_t>(value));
            case CUBE_SESSION_CONNECTED:
                return mem::write<uint8_t>(gc + off::kConnectedOff, static_cast<uint8_t>(value));
            default: return false;
        }
    }

    bool setUiField(int32_t fieldId, int32_t open)
    {
        uintptr_t gc = 0;
        if (!resolveGameController(gc))
            return false;

        const uint8_t on = open ? 1 : 0;
        switch (fieldId)
        {
            case CUBE_UI_INVENTORY:
                return setWidgetOpen(gc, off::kWidgetInventoryOff, on);
            case CUBE_UI_CHARACTER:
                return setWidgetOpen(gc, off::kWidgetCharacterOff, on);
            case CUBE_UI_MAP:
                return setWidgetOpen(gc, off::kWidgetMapOff, on);
            case CUBE_UI_OBJECTIVE:
                return mem::write<uint8_t>(gc + off::kObjectiveOpenByteOff, on);
            default: return false;
        }
    }
}