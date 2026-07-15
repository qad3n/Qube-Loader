#include "hooks/render_dispatch.h"
#include "core/log.h"

#include <windows.h>
#include <mutex>
#include <vector>

namespace hooks::render
{
    namespace
    {
        constexpr char kCategory[] = "render";
        constexpr DWORD kDrainMs = 32; // let an in-flight frame finish before a removed callback is freed

        struct Entry
        {
            Token token;
            d3d9::Callbacks callbacks;
        };

        std::mutex g_mutex;
        std::vector<Entry> g_subscribers;
        Token g_nextToken = 0;

        std::vector<Entry> snapshot()
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            return g_subscribers;
        }

        void dispatchRender(IDirect3DDevice9* device)
        {
            const std::vector<Entry> entries = snapshot();
            for (const Entry& entry : entries)
            {
                if (entry.callbacks.onRender)
                    entry.callbacks.onRender(device);
            }
        }

        void dispatchDeviceReset(bool preReset)
        {
            const std::vector<Entry> entries = snapshot();
            for (const Entry& entry : entries)
            {
                if (entry.callbacks.onDeviceReset)
                    entry.callbacks.onDeviceReset(preReset);
            }
        }

        bool dispatchWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            const std::vector<Entry> entries = snapshot();
            bool swallow = false;
            for (const Entry& entry : entries)
            {
                if (entry.callbacks.onWndProc && entry.callbacks.onWndProc(hwnd, msg, wParam, lParam))
                    swallow = true;
            }
            return swallow;
        }

    }

    Token subscribe(const d3d9::Callbacks& callbacks)
    {
        bool needInstall = false;
        Token token = kInvalidToken;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            needInstall = g_subscribers.empty();
            token = g_nextToken++;
            g_subscribers.push_back(Entry{token, callbacks});
        }

        if (needInstall)
        {
            d3d9::Callbacks dispatch;
            dispatch.onRender = &dispatchRender;
            dispatch.onDeviceReset = &dispatchDeviceReset;
            dispatch.onWndProc = &dispatchWndProc;
            if (!d3d9::install(dispatch))
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_subscribers.clear();
                LOGC(Error, kCategory, "D3D9 hook install failed; render dispatch unavailable");
                return kInvalidToken;
            }
            LOGC(Debug, kCategory, "render dispatch armed (first subscriber)");
        }
        return token;
    }

    void unsubscribe(Token token)
    {
        if (token == kInvalidToken)
            return;

        bool nowEmpty = false;
        bool removed = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            for (size_t i = 0; i < g_subscribers.size(); ++i)
            {
                if (g_subscribers[i].token == token)
                {
                    g_subscribers.erase(g_subscribers.begin() + i);
                    removed = true;
                    break;
                }
            }
            nowEmpty = g_subscribers.empty();
        }

        if (!removed)
            return;

        // A frame already dispatching holds a snapshot of the removed callback; wait it out before
        // the owner frees the code behind that pointer.
        Sleep(kDrainMs);

        if (nowEmpty)
        {
            d3d9::remove();
            LOGC(Debug, kCategory, "render dispatch disarmed (last subscriber)");
        }
    }
}