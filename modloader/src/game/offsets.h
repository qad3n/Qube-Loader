#pragma once
// Static addresses from the RE dumps (image base 0x00400000). Rebase with mem::rebase() before use.
#include <cstdint>

namespace off
{

    constexpr uintptr_t kImageBase = 0x00400000;

    // GameController first virtual method.
    constexpr uintptr_t kGameControllerVfunc0 = 0x0040cbd0;

    // Candidate singleton globals; the debug menu dumps these live to identify GC/World/player.
    constexpr uintptr_t kCandidateA = 0x0076b374;
    constexpr uintptr_t kCandidateB = 0x0076b37c;
    constexpr uintptr_t kCandidateC = 0x0076b380;
    constexpr uintptr_t kCandidateD = 0x0076b3b8;

    constexpr uintptr_t kCandidateGlobals[] = {kCandidateA, kCandidateB, kCandidateC, kCandidateD};

    // Statically linked SQLite: sqlite3_log dispatches (*xLog)(pLogArg, errCode, message); ships null.
    constexpr uintptr_t kSqliteXLog = 0x007666c4;
    constexpr uintptr_t kSqlitePLogArg = 0x007666c8;

    // Local player: [kLocalPlayerPtr] -> GameController*; GC + kLocalPlayerChain -> Creature*.
    // A 0 field offset means the field is not located and is skipped.
    constexpr uintptr_t kNoChain = 0xFFFFFFFF;
    constexpr uintptr_t kLocalPlayerPtr = 0x0076b1c8; // GameController* global
    constexpr uintptr_t kLocalPlayerChain = 0x008006d0; // GC -> local Creature*
    // Creature vftable at object+0; GC+0x8006d0 is uninitialized on the title screen, so confirm it first.
    constexpr uintptr_t kCreatureVtable = 0x006fd8cc;
    constexpr uintptr_t kPlayerHealthOff = 0x16c; // float HP
    constexpr uintptr_t kPlayerLevelOff = 0x190; // int level
    constexpr uintptr_t kPlayerXpOff = 0x194; // int xp
    constexpr uintptr_t kPlayerTypeOff = 0x64; // int entity/model type
    constexpr uintptr_t kPlayerClassOff = 0x140; // byte class 1..4
    constexpr uintptr_t kPlayerSpecOff = 0x141; // byte specialization
    constexpr uintptr_t kPlayerNameOff = 0x1168; // inline null-terminated name
    constexpr bool kPlayerNameIsWide = false; // narrow (not wide) string
    constexpr uintptr_t kPlayerPosXOff = 0x10; // int64 fixed-point, /65536
    constexpr uintptr_t kPlayerPosYOff = 0x18; // int64
    constexpr uintptr_t kPlayerPosZOff = 0x20; // int64 (height)
    constexpr double kPlayerPosScale = 65536.0; // 2^16 fixed-point divisor
    constexpr uint32_t kPlayerStructMinSize = 0x1a0; // core fields; name guarded separately

    // Player action-state, drives the polled PLAYER_ATTACK / PLAYER_JUMP edges.
    // +0x68 is the current action/animation id; +0x6c is elapsed-ms in that action (resets to 0 on
    // any action change). The combat state machine (CombatBehavior::vfunc_0 0x42cb20) writes +0x68
    // for every swing/shot/cast/ability; the exact id is weapon/class dependent, so instead of an
    // attack-id whitelist we use the game's own windup switch (getAttackWindup 0x43caa0): it returns
    // 0 only for these idle/locomotion ids plus the block/eat/downed/knockdown states below. Any
    // other id is an in-progress combat action. (Old kActionAttackA/B 0x50/0x51 were MONSTER-ONLY
    // melee ids the local player never uses, so the attack edge could never fire.)
    constexpr uintptr_t kPlayerActionOff = 0x68; // byte action/animation id
    constexpr uintptr_t kPlayerActionElapsedOff = 0x6c; // s32 ms elapsed in the current action; the
    // game re-zeroes it at the START of every individual swing/shot, so a reset while +0x68 is an
    // attack id is the reliable "a new attack just began" edge (survives chained combos, where +0x68
    // never returns to idle between swings).
    constexpr uint8_t kActionIdle = 0x00;
    constexpr uint8_t kActionEat = 0x6a; // set while eating a consumable/food
    // NOTE: 0x30 and 0x65 are NOT downed states - the game's icon map names them Intercept and
    // Bulwark (abilities). The real downed/knockdown state is kActionKnockdown (0x6e).
    // The player's attack ids are weapon/class dependent, so rather than guess an attack-id
    // whitelist we use the game's own classifier: getAttackWindup(creature, kUseCurrentAction)
    // returns 0 only for idle/locomotion ids and nonzero for any real swing/shot/cast/ability.
    // int __thiscall(Creature* this, int action); action < 0 means "use current +0x68".
    constexpr uintptr_t kGetAttackWindupFn = 0x0043caa0;
    constexpr int32_t kUseCurrentAction = -1;
    // Equipped-weapon type cache on the Creature, used to classify melee/ranged/cast.
    constexpr uintptr_t kWeaponSubtypeOff = 0xaa9; // u8 weapon subtype
    constexpr uint8_t kWeaponSubStaff = 0x05; // cast
    // Contact-flags byte; bit0 = floor contact (clear = airborne).
    constexpr uintptr_t kPlayerContactFlagsOff = 0x5c;
    constexpr uint8_t kGroundContactBit = 0x01; // bit0 = on floor
    // Movement mode resolved from the contact byte (+0x5c) and state-flags u16 (+0x124).
    constexpr uint8_t kContactSwimBit = 0x02; // +0x5c bit1 = in fluid
    constexpr uint8_t kContactClimbBit = 0x04; // +0x5c bit2 = climbable-surface contact
    constexpr uintptr_t kPlayerStateFlagsOff = 0x124; // u16 state flags
    constexpr uint16_t kClimbEnableBit = 0x0001; // +0x124 bit0 = climb/grab active
    constexpr uint8_t kActionBlock = 0x6b; // block/guard action; unmapped ids resolve to UNKNOWN

    // Item POD (0x118 bytes).
    constexpr uintptr_t kItemStructSize = 0x118;
    // Item type ids (value @kItemTypeOff). From the item-name registry populated in the World ctor.
    constexpr int32_t kItemTypeEmpty = 0x0;
    constexpr int32_t kItemTypeConsumable = 0x1; // potions + edible consumables (Cookie/LifePotion/...)
    constexpr int32_t kItemTypeWeapon = 0x3;
    constexpr int32_t kItemTypeSpecial = 0xb; // crafting material / monster part
    constexpr int32_t kItemTypeCoins = 0xc;
    constexpr int32_t kItemTypeCurrency = 0xd;
    constexpr int32_t kItemTypeFood = 0x14; // food; also the pet-taming / pet-food item
    constexpr int32_t kItemTypeAccessory = 0x15; // accessory / medical (Medicine/Antivenom/Bandage/...)
    constexpr int32_t kItemTypeVehicle = 0x17; // HangGlider / Boat
    // Food/consumable magnitude (heal or nourishment) is the level field @0x10; eating sets the
    // action byte to kActionEat. Pet-taming uses a Food item (type 0x14) on a passive tameable
    // species (game predicate FUN_00444680: species in kPassiveSpecies AND state +0x7e & 0x1a00 == 0).
    constexpr uintptr_t kItemTypeOff = 0x0; // u8 type/category
    constexpr uintptr_t kItemSubtypeOff = 0x1; // u8 subtype
    constexpr uintptr_t kItemSeedOff = 0x4; // u32 seed / stat-roll variance
    constexpr uintptr_t kItemModifierOff = 0xc; // u8 modifier / plus-enchant (numeric)
    constexpr uintptr_t kItemMaterialOff = 0xd; // u8 material (Iron/Wood/Gold/...)
    constexpr uintptr_t kItemLevelOff = 0x10; // s16 level (or stack count for consumables)
    constexpr uintptr_t kItemUpgradeCountOff = 0x114; // s32 upgrade/spirit count
    // Equipment: inline array of bare Item structs on the Creature, 12 slots (weapon=idx5, offhand=idx6).
    constexpr uintptr_t kEquipmentBaseOff = 0x418;
    constexpr uintptr_t kEquipmentStride = kItemStructSize;
    constexpr int32_t kEquipmentSlotCount = 12;
    constexpr int32_t kEquipWeaponSlot = 5;
    // Inventory: std::vector<std::vector<InventoryCell>>; cell = [int stack count][Item 0x118].
    constexpr uintptr_t kInventoryVecOff = 0x11dc; // outer vector begin (end=+4, cap=+8)
    constexpr uintptr_t kInventoryOuterStride = 0xc; // inner std::vector size
    constexpr uintptr_t kInventoryCellStride = 0x11c; // [count][Item]
    constexpr uintptr_t kInventoryCellItemOff = 0x4; // Item body inside a cell
    constexpr int32_t kInventoryMaxOuter = 32; // safety cap on tab/page walk
    constexpr int32_t kInventoryMaxInner = 256; // safety cap on cells per tab
    // Held / cursor item (count>0 means occupied). The 0x118 body ends at coins (+0x1304).
    constexpr uintptr_t kHeldItemCountOff = 0x11e8; // int: nonzero when an item is on the cursor
    constexpr uintptr_t kHeldItemOff = 0x11ec; // Item body held on the cursor
    // An item whose type/subtype/material has no model renders as "?" and crashes the game on draw;
    // editors clamp/reject to keep every edit renderable.
    constexpr int32_t kItemDefaultSubtype = 0; // non-weapon items are single-variant
    constexpr int32_t kItemModifierMin = 0;
    constexpr int32_t kItemModifierMax = 4;
    constexpr int32_t kItemLevelMin = 1;
    constexpr int32_t kItemLevelMax = 500;
    constexpr int32_t kItemUpgradeMin = 0;
    constexpr int32_t kItemUpgradeMaxOneHand = 16; // 1H weapons + armor
    constexpr int32_t kItemUpgradeMaxTwoHand = 32; // 2H weapons
    constexpr int32_t kItemStackMin = 1; // a 0/negative stack empties or corrupts the cell
    constexpr int32_t kItemStackMax = 99;
    constexpr int32_t kTwoHandedWeaponSubtypes[] = {5, 6, 7, 8, 10, 11, 15, 16, 17, 18};
    constexpr uintptr_t kSkillRanksOff = 0x1138; // int32[11] skill points-spent array
    constexpr int32_t kSkillRankCount = 11;
    // Item coin value: int __thiscall(Item* this). Buy price = value; sell price = value/2 (min 1).
    constexpr uintptr_t kItemValueFn = 0x004c76e0;

    constexpr uintptr_t kPlayerManaOff = 0x170; // float class resource, 0..1
    constexpr uintptr_t kPlayerStaminaOff = 0x1194; // float stamina, 0..1
    constexpr uintptr_t kPlayerCoinsOff = 0x1304; // int money / coins
    constexpr uintptr_t kPlayerVelXOff = 0x34; // float velocity x (y=+4, z=+8)

    // Combat stats on the Creature.
    constexpr uintptr_t kPlayerPowerOff = 0x180; // float spell/attack power base
    constexpr uintptr_t kPlayerArmorOff = 0x184; // float armor/defense base
    constexpr uintptr_t kPlayerSpiritOff = 0x188; // float spirit base
    constexpr uintptr_t kPlayerComboOff = 0x1164; // int combo / rune counter
    constexpr uintptr_t kPlayerAttackCooldownOff = 0x144; // float attack windup (ready <= 0)
    constexpr uintptr_t kPlayerHitStunOff = 0x128; // int hit-stun/stun-lock timer 0..600
    // hitStun is set to this max on a heavy hit, counts down; gates acting while > 0.
    constexpr int32_t kHitStunMax = 600;
    constexpr uintptr_t kPlayerAttackSpeedOff = 0x17c; // float attack timescale (higher = faster)
    // +0x1190 is the STEALTH/sneak stat (0..1): it reduces enemy detection range and decays when
    // hit. It ALSO feeds crit chance (crit = base + stealth * kCritChancePerPoint), so the same
    // field backs both "sneaking" and the crit contribution. There is no separate stored crit stat.
    constexpr uintptr_t kPlayerStealthOff = 0x1190; // float stealth/sneak stat (also feeds crit)
    constexpr float kStealthMax = 1.0f; // full stealth (normalized 0..1); written to toggle sneak on
    constexpr float kCritChancePerPoint = 0.15f; // crit-chance contribution of one stealth point
    constexpr float kPercentScale = 100.0f; // fraction (0..1) -> percent

    // Status/buff intrusive circular list: head @+0x1178, count @+0x117c; node next@+0, type@+8,
    // magnitude f32@+0xc, duration ms@+0x10.
    constexpr uintptr_t kBuffListHeadOff = 0x1178;
    // The inline name field is exactly 16 bytes (bounded by the buff-list head that follows it);
    // name writes MUST clamp to this or they corrupt the buff list.
    constexpr uintptr_t kPlayerNameCapacity = kBuffListHeadOff - kPlayerNameOff; // 16 bytes
    constexpr uintptr_t kBuffNodeNextOff = 0x0;
    constexpr uintptr_t kBuffNodeTypeOff = 0x8;
    constexpr uintptr_t kBuffNodeMagnitudeOff = 0xc;
    constexpr uintptr_t kBuffNodeDurationOff = 0x10;
    constexpr int32_t kMaxBuffWalk = 64; // safety cap on the effect list

    // State word @+0x7e is a u16 flag field. Bit 0x200 is the LANTERN / held-light RENDER flag
    // (read by render vfunc_11 0x4ac260 + getScaleVec4 0x4460f0), NOT crouch/sneak. Cube World has
    // no crouch bit; "sneak" is the Rogue stealth ability whose stealth magnitude is a float at
    // kPlayerStealthOff. The 0x1a00 mask on this word gates the passive/wildlife faction.
    constexpr uintptr_t kStateWordOff = 0x7e;
    constexpr uint16_t kLanternBit = 0x200;

    // World is embedded in the GameController at +0x2e4. Climate walks World->Region->Zone->ZoneTile
    // from the player tile; no biome name is stored (biome is computed from temp/humidity).
    constexpr uintptr_t kWorldPtr = 0x0076b1c8; // GameController* global
    constexpr uint32_t kWorldStructMinSize = 0x2c8;
    constexpr uintptr_t kWorldEmbedOff = 0x2e4; // GC + this = embedded World
    constexpr int32_t kPosToTileShift = 16; // int64 pos >> 16 = tile coord
    constexpr int32_t kTileToZoneShift = 8; // tile >> 8 = zone (256 tiles/zone)
    constexpr int32_t kZoneToRegionShift = 6; // zone >> 6 = region (64 zones/region)
    constexpr uintptr_t kRegionGridOff = 0xbc; // World + this + idx*4 -> Region*
    constexpr int32_t kRegionGridDim = 0x400; // 1024x1024 region grid
    constexpr uintptr_t kZoneGridOff = 0x10018; // Region + this + idx*4 -> Zone*
    constexpr int32_t kZoneGridDim = 0x40; // 64x64 zone grid (mask 0x3f)
    constexpr uintptr_t kZoneTileBaseOff = 0xa8; // *(Zone + this) = tile array base
    constexpr int32_t kTileDim = 0x100; // 256x256 tiles/zone (mask 0xff)
    constexpr uintptr_t kTileStride = 0x20; // ZoneTile size
    constexpr uintptr_t kTileTerrainOff = 0x0; // byte terrain-type id
    constexpr uintptr_t kTileTempOff = 0x4; // float temperature
    constexpr uintptr_t kTileHumidityOff = 0x8; // float humidity
    constexpr uintptr_t kTileElevationOff = 0x10; // int32 base terrain height
    constexpr uintptr_t kTimeOfDayOff = 0x800440; // GC-relative: time-of-day ms
    constexpr int32_t kDayStartHour = 6; // [6,20) counts as daytime
    constexpr int32_t kNightStartHour = 20;
    // Bound spawn on the player Creature: int32 zone-coord pair (>>6 == region), no stored Z.
    // Two candidate pairs exist; flag-gated.
    constexpr uintptr_t kSpawnFlagOff = 0x1d8; // byte: spawn bound / type flag
    constexpr uintptr_t kSpawnZoneXOff = 0x1dc; // int32 zone X (candidate B)
    constexpr uintptr_t kSpawnZoneYOff = 0x1e0; // int32 zone Y (candidate B)
    constexpr uintptr_t kSpawnZoneXAltOff = 0x1b0; // int32 zone X (candidate A)
    constexpr uintptr_t kSpawnZoneYAltOff = 0x1b4; // int32 zone Y (candidate A)
    constexpr uintptr_t kWorldSeedOff = 0x800448; // GC-relative world seed
    // Per-zone placed-structure vector: begin @Zone+0xc (end +4), stride 0x188; record type@+0,
    // int64 pos@+8. Raw type id.
    constexpr uintptr_t kZoneStructVecOff = 0xc;
    constexpr uintptr_t kZoneStructStride = 0x188;
    constexpr int32_t kZoneStructMaxWalk = 256; // safety cap
    constexpr uintptr_t kStructTypeOff = 0x0; // int kind/type id
    constexpr uintptr_t kStructPosXOff = 0x8; // int64 fixed-point (y=+0x10, z=+0x18)
    // Camera view/projection 4x4 float matrices (GC-relative).
    constexpr uintptr_t kViewMatrixOff = 0x800d48;
    constexpr uintptr_t kProjMatrixOff = 0x800d88;
    constexpr int32_t kMatrixFloats = 16;

    // Entity registry std::map<uint64,Creature*> (MSVC _Tree) embedded in the GC; walk from
    // _Myhead, node value @+0x18.
    constexpr uintptr_t kEntityMapHead = 0x2e8; // GC: _Myhead (nil sentinel)
    constexpr uintptr_t kRbLeft = 0x00; // node _Left
    constexpr uintptr_t kRbParent = 0x04; // node _Parent
    constexpr uintptr_t kRbRight = 0x08; // node _Right
    constexpr uintptr_t kRbKey = 0x10; // node key (uint64 entity id)
    constexpr uintptr_t kRbValue = 0x18; // node value (Creature*)
    constexpr uint32_t kMaxEntityWalk = 4096; // safety cap on tree traversal
    // Creature "kind" byte +0x60 (entity-role): 0 player, 1 monster, 5 pet, 6 npc. Value 3 exists
    // but is unlabelled in the binary, so it is left unclassified.
    constexpr uintptr_t kEntityKindOff = 0x60;
    constexpr int32_t kKindPlayer = 0;
    constexpr int32_t kKindMonster = 1; // creature faction (monsters AND passive animals)
    constexpr int32_t kKindPet = 5;
    constexpr int32_t kKindNpc = 6; // non-combatant / passive (never hostile)
    // Species the game treats as the peaceful-critter faction: a kind==1 in this set is passive
    // wildlife, not a hostile monster (gated on state word +0x7e & 0x1a00 == 0).
    constexpr uint16_t kPassiveFactionStateMask = 0x1a00;
    constexpr int32_t kSpeciesCollie = 0x13;
    constexpr int32_t kSpeciesShepherdDog = 0x14;
    constexpr int32_t kSpeciesAlpaca = 0x16;
    constexpr int32_t kSpeciesBrownAlpaca = 0x17;
    constexpr int32_t kSpeciesTurtle = 0x19;
    constexpr int32_t kSpeciesTerrier = 0x1a;
    constexpr int32_t kSpeciesScottishTerrier = 0x1b;
    constexpr int32_t kSpeciesCat = 0x1e;
    constexpr int32_t kSpeciesBrownCat = 0x1f;
    constexpr int32_t kSpeciesWhiteCat = 0x20;
    constexpr int32_t kSpeciesPig = 0x21;
    constexpr int32_t kSpeciesSheep = 0x22;
    constexpr int32_t kSpeciesBunny = 0x23;
    constexpr int32_t kSpeciesHornet = 0x35;
    constexpr int32_t kSpeciesCrow = 0x37;
    constexpr int32_t kSpeciesChicken = 0x38;
    constexpr int32_t kSpeciesSeagull = 0x39;
    constexpr int32_t kSpeciesParrot = 0x3a;
    constexpr int32_t kSpeciesPeacock = 0x43;
    constexpr int32_t kSpeciesFrog = 0x44;
    constexpr int32_t kSpeciesDuckbill = 0x4a;
    constexpr int32_t kSpeciesMole = 0x57;
    constexpr int32_t kSpeciesOwl = 0x5c;
    constexpr int32_t kSpeciesPenguin = 0x5d;
    constexpr int32_t kSpeciesHorse = 0x62;
    constexpr int32_t kSpeciesCow = 0x64;
    constexpr int32_t kSpeciesCrab = 0x6a;
    constexpr int32_t kSpeciesSeaCrab = 0x6b;
    constexpr int32_t kSpeciesKoala = 0x59;
    constexpr int32_t kSpeciesSapphireFish = 0x91;
    constexpr int32_t kSpeciesLemonFish = 0x92;
    constexpr int32_t kSpeciesSeahorse = 0x93;
    constexpr int32_t kPassiveSpecies[] =
    {
        kSpeciesCollie, kSpeciesShepherdDog, kSpeciesAlpaca, kSpeciesBrownAlpaca, kSpeciesTurtle,
        kSpeciesTerrier, kSpeciesScottishTerrier, kSpeciesCat, kSpeciesBrownCat, kSpeciesWhiteCat,
        kSpeciesPig, kSpeciesSheep, kSpeciesBunny, kSpeciesHornet, kSpeciesCrow, kSpeciesChicken,
        kSpeciesSeagull, kSpeciesParrot, kSpeciesPeacock, kSpeciesFrog, kSpeciesDuckbill,
        kSpeciesMole, kSpeciesOwl, kSpeciesPenguin, kSpeciesHorse, kSpeciesCow, kSpeciesCrab,
        kSpeciesSeaCrab, kSpeciesKoala, kSpeciesSapphireFish, kSpeciesLemonFish, kSpeciesSeahorse
    };
    // Star/power rank +0x1a8 (byte): stat-scaling magnitude ("stars"). There is no rarity/elite
    // field; +0x140 is the class byte for every creature.
    constexpr uintptr_t kEntityRankOff = 0x1a8;
    // Body yaw (radians) +0x118c grows unbounded (never wrapped by the game); readers MUST
    // mathutil::wrapRadians it. Player facing uses the camera yaw, which stays live while airborne.
    constexpr uintptr_t kCreatureBodyYawOff = 0x118c;
    // Species the game treats as boss/large creatures (single-lookup membership table).
    constexpr int32_t kBossSpeciesA = 0x65;
    constexpr int32_t kBossSpeciesB = 0x6b;
    constexpr int32_t kBossSpeciesC = 0x6c;
    constexpr int32_t kBossSpeciesD = 0x6d;
    constexpr int32_t kBossSpeciesE = 0x72;
    constexpr int32_t kBossSpeciesF = 0x73;
    constexpr int32_t kBossSpeciesG = 0x74;
    constexpr int32_t kBossSpeciesH = 0x75;
    constexpr int32_t kBossSpeciesI = 0x76;
    constexpr int32_t kBossSpeciesJ = 0x77;
    constexpr int32_t kBossSpecies[] = {
        kBossSpeciesA, kBossSpeciesB, kBossSpeciesC, kBossSpeciesD, kBossSpeciesE,
        kBossSpeciesF, kBossSpeciesG, kBossSpeciesH, kBossSpeciesI, kBossSpeciesJ};
    constexpr uintptr_t kSelectedEntityOff = 0x8008d8; // GC: selected/use-target Creature*

    // Active pet's entity id (0 = none); resolve against the entity map.
    constexpr uintptr_t kPetIdOff = 0x11c8;

    // Camera + display, GameController-relative.
    constexpr uintptr_t kCamDistanceOff = 0x1c0; // float 3rd-person zoom [0,14], def 5
    constexpr uintptr_t kCamPitchOff = 0x1b0; // float pitch [0,180] degrees
    constexpr uintptr_t kCamYawOff = 0x1b8; // float yaw (degrees); also the stable facing
    constexpr float kCamFovRadians = 0.785398f; // hardcoded pi/4 (45 deg), not stored
    constexpr uintptr_t kRenderWidthOff = 0x11c; // int32 backbuffer width
    constexpr uintptr_t kRenderHeightOff = 0x120; // int32 backbuffer height
    // Global settings struct @0x76b1d8 (12x int32 from options.cfg).
    constexpr uintptr_t kFullscreenFlag = 0x0076b1d8; // static global int: fullscreen
    constexpr uintptr_t kSettingResX = 0x0076b1dc; // int resolution X
    constexpr uintptr_t kSettingResY = 0x0076b1e0; // int resolution Y
    constexpr uintptr_t kSettingRenderDistance = 0x0076b1e8; // int view/render distance
    constexpr uintptr_t kSettingSoundVolume = 0x0076b1ec; // int sound volume
    constexpr uintptr_t kSettingMusicVolume = 0x0076b1f0; // int music volume
    constexpr uintptr_t kSettingMinTimeStep = 0x0076b204; // int min ms/frame (implicit fps cap)

    // Session / network state on the GameController.
    constexpr uintptr_t kNetModeOff = 0x8009b0; // byte: 0 singleplayer, 1 multiplayer
    constexpr uintptr_t kConnectedOff = 0x800585; // byte: networking active

    // UI widget-open: Widget* -> (+0x3c) -> (+0x68 index, +0x94 array); array[index] != 0 = visible.
    constexpr uintptr_t kWidgetInventoryOff = 0x8008c0; // GC: InventoryWidget*
    constexpr uintptr_t kWidgetCharacterOff = 0x8008bc; // GC: Character/Skill widget*
    constexpr uintptr_t kWidgetMapOff = 0x800910; // GC: map/skill widget*
    constexpr uintptr_t kObjectiveOpenByteOff = 0x8008f1; // GC: objective panel open byte
    constexpr uintptr_t kWidgetStateOff = 0x3c; // Widget -> state object
    constexpr uintptr_t kWidgetIndexOff = 0x68; // state -> current index
    constexpr uintptr_t kWidgetArrayOff = 0x94; // state -> visibility array base
    constexpr int32_t kWidgetMaxIndex = 256; // sanity bound on a garbage index

    // Crosshair aim/hover target entity id (what the player is looking at), distinct from the
    // committed selection @kSelectedEntityOff.
    constexpr uintptr_t kAimTargetIdOff = 0x800a70; // GC-relative uint64

    // Select/use action (R key): __thiscall GameController::updateSelectedEntity; resolves the aim id
    // to a creature, commits it to kSelectedEntityOff, then dispatches on the target's +0x140
    // discriminator (opens the container/dialog/widget UI).
    constexpr uintptr_t kUpdateSelectedEntityFn = 0x004889e0;
    // Selection discriminator at target+kPlayerClassOff: a creature carries class 1..4, an object
    // carries one of these.
    constexpr int32_t kSelectContainerLo = 0x80; // loot/trade container
    constexpr int32_t kSelectContainerHi = 0x82;
    constexpr int32_t kSelectDialog = 0x83; // dialog / select widget
    constexpr int32_t kSelectWidget = 0x89; // other select widget

    // Item pickup (E / hold-to-pickup): __thiscall GameController::onItemPickup, fired once when the
    // hold completes. Reads the staged item POD the game copied in before the call, moves it into the
    // player inventory (or credits coins), plays pickup.wav.
    constexpr uintptr_t kOnItemPickupFn = 0x004709c0;
    // Staged pickup buffer, GC-relative: a cell with the item POD @+0, stack count @+kItemStructSize
    // (0x118), a contents vector @+0x11c. Valid to read for the duration of the onItemPickup call.
    constexpr uintptr_t kStagedPickupItemOff = 0x1000e7c;

    // Game-function hook targets (mod-facing interception via src/game/gamehooks).
    // CombatBehavior::vfunc_0 impact handler; __thiscall(self, victim, hitCtx, damage, flags) void.
    // Cancel negates the whole hit; damage-rescale is best-effort.
    constexpr uintptr_t kImpactFn = 0x0042cb20;
    // Crit roll on the attacker. __thiscall, no args, returns a crit bool in AL.
    constexpr uintptr_t kCritRollFn = 0x004444a0;
    // Compute-max-health; __thiscall returns max-HP FLOAT in ST0 (detour MUST be typed float).
    constexpr uintptr_t kMaxHealthFn = 0x00444db0;

    constexpr uintptr_t kPlayerBaseDamageOff = 0x178; // float base-damage input (pre-multiplier)

    // Per-Creature ability cooldown map std::map<int,int> at +0x139c (_Myhead ptr, _Mysize @+0x13a0);
    // key abilityId @+0x10, value remaining ms @+0x14. Populated only after an ability is used.
    constexpr uintptr_t kAbilityCdMapHeadOff = 0x139c;
    constexpr uintptr_t kAbilityCdKeyOff = 0x10;
    constexpr uintptr_t kAbilityCdValueOff = 0x14;
    constexpr uint32_t kMaxAbilityWalk = 256; // safety cap on the cooldown-map traversal

    // Knockdown = action id 0x6e; stagger timer is hitStun @+0x128; knockback velocity floats
    // @+0x11d0/+0x11d4.
    constexpr uint8_t kActionKnockdown = 0x6e;
    constexpr uintptr_t kKnockbackVelXOff = 0x11d0;
    constexpr uintptr_t kKnockbackVelYOff = 0x11d4;

    // Effective combat power = level/2 + star + 1 (does the entity outclass the player).
    constexpr int32_t kEffectivePowerLevelDiv = 2;
    constexpr int32_t kEffectivePowerBase = 1;
    // A monster is surfaced as "elite" at this star rank or above, or if a boss species (derived).
    constexpr int32_t kEliteStarRank = 3;

    // Sound-effect playback (GameController methods, __thiscall this=GC):
    // 2D at the listener: void(int sfxId); positional: void(int sfxId, int64* pos, float vol, float pitch).
    // pos is an int64[3] world position (fixed-point, kPlayerPosScale). ids: CUBE_CATALOG_SOUND (0..100).
    constexpr uintptr_t kPlaySound2DFn = 0x00484320;
    constexpr uintptr_t kPlaySoundPosFn = 0x00484350;
    constexpr int32_t kSoundIdMin = 0;
    constexpr int32_t kSoundIdMax = 100; // 101 built-in sfx ids (0x00..0x64)
    // XAudio2Engine vtable slots (methods on GC+kAudioEngineOff), __thiscall this=engine.
    constexpr uintptr_t kAudioVtblStopMusic = 0x0c; // slot 3: void() stop both music streamers
    constexpr uintptr_t kAudioVtblSetMusicVolume = 0x14; // slot 5: void(float a, float b) live music volume
    // Audio: XAudio2Engine* @GC+0x800714; live music state on a Music streamer at engine+0x24.
    // Play/stop/setVolume are engine-vtable game-calls.
    constexpr uintptr_t kAudioEngineOff = 0x800714; // GC-relative XAudio2Engine*
    constexpr uintptr_t kAudioStreamerAOff = 0x24; // engine-relative primary Music streamer*
    constexpr uintptr_t kMusicPlayingOff = 0x1e0010; // Music streamer: playing u8
    constexpr uintptr_t kMusicLoopingOff = 0x1e0011; // Music streamer: looping u8
    constexpr uintptr_t kMusicVolumeOff = 0x1e0018; // Music streamer: live volume f32

}
