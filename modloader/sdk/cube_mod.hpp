#pragma once
// Ergonomic C++ layer over the C ABI (cube_sdk.h): a mod never sees an address; offsets and memory
// access all live in the loader. This is the umbrella header a mod includes; it pulls in every
// cube/ accessor header (split per domain) and the CUBE_MOD entry macro.

#include "cube/common.hpp"
#include "cube/hero.hpp"
#include "cube/world.hpp"
#include "cube/pet.hpp"
#include "cube/view.hpp"
#include "cube/items.hpp"
#include "cube/session.hpp"
#include "cube/entity.hpp"
#include "cube/selection.hpp"
#include "cube/hookcall.hpp"
#include "cube/logger.hpp"
#include "cube/config.hpp"
#include "cube/storage.hpp"
#include "cube/locale.hpp"
#include "cube/assets.hpp"
#include "cube/events.hpp"
#include "cube/mod.hpp"

// Opt-in ImGui overlay layer (cube::Menu + mod.menu()). Auto-included ONLY when the mod builds with
// ImGui on its include path - which a menu mod gets from the SDK's cube_add_imgui CMake helper. This
// keeps imgui.h out of mods that draw nothing, while a menu mod just includes cube_mod.hpp and calls
// mod.menu(). A mod can also include "cube/menu.hpp" explicitly.
#if defined(__has_include) && __has_include("imgui.h")
#include "cube/menu.hpp"
#endif
