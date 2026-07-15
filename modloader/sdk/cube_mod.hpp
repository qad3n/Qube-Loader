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
#include "cube/events.hpp"
#include "cube/mod.hpp"
