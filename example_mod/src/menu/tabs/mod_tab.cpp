#include "menu/tabs/mod_tab.h"
#include "mod_context.h"
#include "overlay.h"
#include "features/memory_probe.h"
#include "features/services_demo.h"
#include "features/locale_demo.h"

#include "cube_mod.hpp"
#include "imgui.h"

#include <cstdio>

namespace exmod::menu
{
    namespace
    {

        constexpr char kDefaultLogMessage[] = "hello from the mod menu";

        const char* const kLogLevelNames[] = {"Trace", "Debug", "Info", "Warn", "Error"};

        // config() keys: these persist REAL example_mod settings across restarts (not throwaway demo
        // values) - the UI scale, log level, and the on-load greeting.
        constexpr char kUiScaleKey[] = "ui_scale";
        constexpr char kLogLevelKey[] = "log_level";
        constexpr char kGreetOnLoadKey[] = "greet_on_load";
        constexpr char kGreetingKey[] = "greeting";
        constexpr char kGreetingDefault[] = "welcome back";

        // storage() keys: mod-owned binary save data (the launch counter and a free-text note blob).
        constexpr char kLaunchesKey[] = "launches";
        constexpr char kNoteKey[] = "note";

        struct CapabilityName
        {
            unsigned bit;
            const char* name;
        };

        constexpr CapabilityName kCapabilityNames[] = {
            {CUBE_CAP_RAW_MEM, "RawMem"},
            {CUBE_CAP_RAW_HOOKS, "RawHooks"},
            {CUBE_CAP_WRITES, "Writes"},
            {CUBE_CAP_OVERLAY, "Overlay"},
        };

        // Formats the declared capability bitset as "RawMem | Writes | ..." (or the unrestricted note)
        // so the Info tab documents what the loader gates this mod against.
        void formatCapabilities(unsigned caps, char* out, int size)
        {
            if (caps == 0)
            {
                std::snprintf(out, size, "(none declared - unrestricted)");
                return;
            }
            int written = 0;
            for (const CapabilityName& entry : kCapabilityNames)
            {
                if ((caps & entry.bit) == 0)
                    continue;
                const int n = std::snprintf(out + written, size - written, "%s%s", written ? " | " : "", entry.name);
                if (n < 0 || n >= size - written)
                    break;
                written += n;
            }
        }

    }

    ModTab::ModTab()
    {
        std::snprintf(m_log.message, sizeof(m_log.message), "%s", kDefaultLogMessage);

        // Seed every editable field from this mod's persisted config (written last session), and apply
        // the ones with a live effect now so the saved UI scale / log level take hold immediately.
        cube::Config config = cube::mod().config();
        m_uiScale = config.getFloat(kUiScaleKey, overlay::uiScale());
        overlay::setUiScale(m_uiScale);
        m_log.levelIndex = config.getInt(kLogLevelKey, CUBE_LOG_INFO);
        m_greetOnLoad = config.getBool(kGreetOnLoadKey, true);
        std::snprintf(m_greeting, sizeof(m_greeting), "%s",
                      config.getString(kGreetingKey, kGreetingDefault).c_str());

        reloadNote();
    }

    void ModTab::reloadNote()
    {
        cube::Storage storage = cube::mod().storage();
        storage.setScope(m_scope);
        std::snprintf(m_note, sizeof(m_note), "%s", storage.getString(kNoteKey).c_str());
    }

    void ModTab::drawInfo(const CubeEventArgs& frame)
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (beginTable("mod_info"))
        {
            const char* modId = cube::mod().id();
            row("Mod id", "%s", (modId && modId[0]) ? modId : "(stem fallback)");
            char capsText[96];
            formatCapabilities(cube::mod().capabilities(), capsText, sizeof(capsText));
            row("Capabilities", "%s", capsText);
            row("Environment", "%s", cube::mod().isClient() ? "client" : "server");
            row("Loader ABI", "%u", g_api->abiVersion);
            row("SDK ABI", "%d", CUBE_ABI_VERSION);
            row("FPS", "%.1f", io.Framerate);
            row("Frame", "%u", frame.frameIndex);
            row("DPI scale", "%.2fx", overlay::dpiScale());
            ImGui::EndTable();
        }
        // UI scale is persisted through mod.config() (a float setting), so it survives a restart -
        // exactly the pain point config solves (this slider used to reset every launch).
        ImGui::SeparatorText("ui scale (saved to config)");
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::SliderFloat("scale", &m_uiScale, overlay::kMinUiScale, overlay::kMaxUiScale, "%.2fx", kClampFlags))
        {
            overlay::setUiScale(m_uiScale);
            cube::mod().config().setFloat(kUiScaleKey, m_uiScale);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset##scale"))
        {
            m_uiScale = 1.0f;
            overlay::setUiScale(m_uiScale);
            cube::mod().config().setFloat(kUiScaleKey, m_uiScale);
        }

        ImGui::SeparatorText("input");
        bool allowGameInput = overlay::allowGameInput();
        if (ImGui::Checkbox("Allow input and movement in menu", &allowGameInput))
            overlay::setAllowGameInput(allowGameInput);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Keep moving and looking with the menu open. The menu becomes a display-only\n"
                              "HUD and the game grabs the cursor, so widgets are not clickable in this mode.\n"
                              "Press INSERT or DELETE to close and return to the interactive menu.");
    }

    void ModTab::drawMemory()
    {
        // All parse/rebase/read/write logic lives in MemoryProbe; this view only binds + displays.
        MemoryProbe& probe = memoryProbe();
        ImGui::TextDisabled("mod.rebase / readable / read<T> (all loader guarded)");
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::InputText("static addr##mem", probe.addressBuffer(), probe.bufferSize(),
                         ImGuiInputTextFlags_CharsHexadecimal);
        const bool readable = probe.readable();
        if (beginTable("mod_mem"))
        {
            row("Static", "0x%08X", probe.staticAddress());
            row("Rebased", "0x%08X", probe.runtimeAddress());
            row("Readable", "%s", yesNo(readable));
            if (readable)
                row("u32 @addr", "0x%08X", probe.readU32());
            ImGui::EndTable();
        }

        ImGui::SeparatorText("mod.write<T> (guarded)");
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::InputText("value hex##memw", probe.valueBuffer(), probe.bufferSize(),
                         ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::Checkbox("arm write", &probe.writeArmed());
        ImGui::SameLine();
        ImGui::BeginDisabled(!probe.writeArmed());
        if (ImGui::Button("Write u32"))
        {
            const bool ok = probe.write();
            emitLog(ok ? CUBE_LOG_INFO : CUBE_LOG_WARN,
                    ok ? "example_mod: mod.write ok" : "example_mod: mod.write blocked");
        }
        ImGui::EndDisabled();
        ImGui::TextColored(kWarnColor, "Writes to the rebased address above. Be careful.");
    }

    void ModTab::drawLogging()
    {
        ImGui::TextDisabled("emit a line at any level via log.write");
        ImGui::SetNextItemWidth(sc(kInputWidth));
        // The chosen level is remembered in mod.config() (an int setting), so it persists across restarts.
        if (ImGui::Combo("level", &m_log.levelIndex, kLogLevelNames, IM_ARRAYSIZE(kLogLevelNames)))
            cube::mod().config().setInt(kLogLevelKey, m_log.levelIndex);
        ImGui::SetNextItemWidth(sc(kTeleportInputWidth));
        ImGui::InputText("message", m_log.message, sizeof(m_log.message));
        if (ImGui::Button("Log message"))
            emitLog(static_cast<CubeLogLevel>(m_log.levelIndex), m_log.message);

        ImGui::SeparatorText("quick");
        if (ImGui::Button("Trace"))
            emitLog(CUBE_LOG_TRACE, "example_mod: trace test line");
        ImGui::SameLine();
        if (ImGui::Button("Debug"))
            emitLog(CUBE_LOG_DEBUG, "example_mod: debug test line");
        ImGui::SameLine();
        if (ImGui::Button("Info"))
            emitLog(CUBE_LOG_INFO, "example_mod: info test line");
        ImGui::SameLine();
        if (ImGui::Button("Warn"))
            emitLog(CUBE_LOG_WARN, "example_mod: warn test line");
        ImGui::SameLine();
        if (ImGui::Button("Error"))
            emitLog(CUBE_LOG_ERROR, "example_mod: error test line");
    }

    void ModTab::drawPersist()
    {
        // config() = user-editable settings (<stem>.ini); storage() = mod-owned binary save data. Both
        // key on this mod's DLL stem and persist across restarts. This tab exercises the whole surface;
        // ui_scale (float) and log_level (int) are edited on the Info / Logging tabs and shown here.
        cube::Config config = cube::mod().config();
        cube::Storage storage = cube::mod().storage();

        ImGui::TextDisabled("mod.config() - settings saved to config/<stem>.ini (all four types)");
        const int levelIndex = (m_log.levelIndex >= 0 && m_log.levelIndex < IM_ARRAYSIZE(kLogLevelNames))
            ? m_log.levelIndex : CUBE_LOG_INFO;
        if (beginTable("mod_cfg"))
        {
            row("ui_scale (float)", "%.2f  [Info tab]", config.getFloat(kUiScaleKey, 1.0f));
            row("log_level (int)", "%s  [Logging tab]", kLogLevelNames[levelIndex]);
            row("greet_on_load (bool)", "%s", yesNo(config.getBool(kGreetOnLoadKey, true)));
            ImGui::EndTable();
        }
        if (ImGui::Checkbox("Greet on load", &m_greetOnLoad))
            config.setBool(kGreetOnLoadKey, m_greetOnLoad);
        ImGui::SetNextItemWidth(sc(kTeleportInputWidth));
        ImGui::InputText("greeting (string)", m_greeting, sizeof(m_greeting));
        ImGui::SameLine();
        if (ImGui::Button("Save##greeting"))
        {
            config.setString(kGreetingKey, m_greeting);
            emitLog(CUBE_LOG_INFO, "example_mod: greeting saved to config");
        }

        // storage() launch counter lives at the unscoped root (as written by example_mod.cpp init).
        ImGui::SeparatorText("mod.storage() - binary save data under data/<stem>/");
        storage.setScope("");
        if (beginTable("mod_launch"))
        {
            row("launch count", "%d", storage.getValue<int>(kLaunchesKey, 0));
            ImGui::EndTable();
        }
        ImGui::TextDisabled("bumped once per game launch (example_mod.cpp init); survives restarts");
        if (ImGui::Button("Reset launch count"))
        {
            storage.setScope("");
            storage.putValue<int>(kLaunchesKey, 0);
            emitLog(CUBE_LOG_INFO, "example_mod: launch count reset");
        }

        // A free-text note blob, optionally namespaced by a scope (setScope) so each save/world keeps
        // its own copy. Demonstrates put/getString, has, size and remove.
        ImGui::SeparatorText("note blob (put/getString + has/size/remove + setScope)");
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::InputText("scope", m_scope, sizeof(m_scope));
        ImGui::SameLine();
        if (ImGui::Button("Apply scope"))
            reloadNote();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Namespaces the note under data/<stem>/<scope>/ (e.g. per world seed or\n"
                              "character). Empty scope is the shared root. Applying reloads the note.");
        storage.setScope(m_scope);
        const bool hasNote = storage.has(kNoteKey);
        const int noteSize = storage.size(kNoteKey);
        ImGui::SetNextItemWidth(sc(kTeleportInputWidth));
        ImGui::InputText("note", m_note, sizeof(m_note));
        if (ImGui::Button("Save##note"))
        {
            storage.putString(kNoteKey, m_note);
            emitLog(CUBE_LOG_INFO, "example_mod: note saved to storage");
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload##note"))
            reloadNote();
        ImGui::SameLine();
        ImGui::BeginDisabled(!hasNote);
        if (ImGui::Button("Delete##note"))
        {
            storage.remove(kNoteKey);
            m_note[0] = '\0';
            emitLog(CUBE_LOG_INFO, "example_mod: note removed from storage");
        }
        ImGui::EndDisabled();
        if (beginTable("mod_note"))
        {
            row("exists (has)", "%s", yesNo(hasNote));
            row("size (bytes)", "%d", noteSize);
            ImGui::EndTable();
        }
    }

    void ModTab::drawServices()
    {
        // Inter-mod ecosystem (ABI 22): example_mod publishes "example.ping", resolves it at READY, and
        // exchanges a directed message. It targets its OWN id here (one DLL); with a second mod the
        // provider/consumer split is identical. See features/services_demo.cpp.
        ServicesDemo& demo = servicesDemo();
        ImGui::TextDisabled("mod.services(): register / query / onMessage / sendMessage");
        if (beginTable("mod_services"))
        {
            row("service resolved (onReady)", "%s", yesNo(demo.serviceResolved()));
            row("ping(21) via query", "%d", demo.lastPingResult());
            row("messages received", "%u", demo.messagesReceived());
            row("last payload -> reply", "%d -> %d", demo.lastPayload(), demo.lastReply());
            ImGui::EndTable();
        }

        ImGui::SeparatorText("send a directed message to self");
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::InputInt("payload", &m_pingValue);
        if (ImGui::Button("Send self ping"))
        {
            m_pingResult = demo.sendSelfPing(m_pingValue);
            emitLog(CUBE_LOG_INFO, "example_mod: sendMessage(self) returned a reply");
        }
        ImGui::SameLine();
        ImGui::Text("reply: %d", m_pingResult);
        ImGui::TextDisabled("the handler replies payload + 1; sendMessage returns that value");
    }

    void ModTab::drawLocale()
    {
        // Localization (ABI 23): translate keys against lang/<stem>/<locale>.ini, switch the active
        // locale live, and fall back to the key when a translation is missing. See features/locale_demo.cpp.
        LocaleDemo& demo = localeDemo();
        ImGui::TextDisabled("mod.locale(): translate / setLocale / getLocale");
        if (beginTable("mod_locale"))
        {
            row("active locale", "%s", demo.locale().c_str());
            row("greeting", "%s", demo.translate("greeting").c_str());
            row("menu_title", "%s", demo.translate("menu_title").c_str());
            row("missing_key", "%s", demo.translate("missing_key").c_str());
            ImGui::EndTable();
        }
        ImGui::TextDisabled("missing_key has no translation, so it falls back to the key text");

        ImGui::SeparatorText("switch locale (lang/example_mod/<locale>.ini)");
        if (ImGui::Button("en"))
            demo.setLocale("en");
        ImGui::SameLine();
        if (ImGui::Button("de"))
            demo.setLocale("de");
        ImGui::SameLine();
        if (ImGui::Button("fr (no file)"))
            demo.setLocale("fr");
        ImGui::TextDisabled("example_mod ships en + de; fr has no file, so every key falls back");
    }

    void ModTab::draw(const CubeEventArgs& frame)
    {
        if (!ImGui::BeginTabBar("##modtabs"))
            return;
        if (ImGui::BeginTabItem("Info"))
        {
            drawInfo(frame);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Memory"))
        {
            drawMemory();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Logging"))
        {
            drawLogging();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Persist"))
        {
            drawPersist();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Services"))
        {
            drawServices();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Locale"))
        {
            drawLocale();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

}
