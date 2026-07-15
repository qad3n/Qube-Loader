#include "game/catalog.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace game
{
    namespace
    {
        // "id=name" pairs, comma-separated, one string per CubeCatalog. Empty = raw id.
        const char* const kCatalogData[CUBE_CATALOG_COUNT] =
        {
            // CUBE_CATALOG_ITEM_TYPE (item POD byte +0x0)
            "1=Consumable,2=Formula,3=Weapon,4=Chest Armor,5=Gloves,6=Boots,7=Shoulder Armor,"
            "8=Amulet,9=Ring,11=Special,12=Coin,13=Platinum Coin,18=Candle,19=Pet,20=Food,"
            "21=Accessory,23=Vehicle,24=Lamp,25=Mana Cube",
            // CUBE_CATALOG_WEAPON_SUBTYPE (item POD byte +0x1 when type==Weapon)
            "0=Sword,1=Axe,2=Mace,3=Dagger,4=Fist,5=Longsword,6=Bow,7=Crossbow,8=Boomerang,"
            "9=Arrow,10=Staff,11=Wand,12=Bracelet,13=Shield,14=Arrows,15=Greatsword,16=Greataxe,"
            "17=Greatmace,20=Torch",
            // CUBE_CATALOG_MATERIAL (item POD byte +0xd)
            "1=Iron,2=Wood,5=Obsidian,7=Bone,10=Copper,11=Gold,12=Silver,13=Emerald,14=Sapphire,"
            "15=Ruby,16=Diamond,17=Sandstone,18=Saurian,19=Parrot,20=Mammoth,21=Plant,22=Ice,"
            "23=Light,24=Glass,25=Silk,26=Linen,27=Cotton,128=Fire,129=Unholy,130=Ice,131=Wind",
            // CUBE_CATALOG_ITEM_MODIFIER (item POD byte +0xc) - numeric, no name table
            "",
            // CUBE_CATALOG_TERRAIN (ZoneTile byte +0x0) - numeric material index, no names
            "",
            // CUBE_CATALOG_BUFF_TYPE (status node byte +0x8) - only proven ids
            "1=Damage Reduction,6=Shield,8=Unstoppable,11=Guaranteed Crit",
            // CUBE_CATALOG_ACTION (Creature byte +0x68): full action/animation id -> name from the
            // game's own icon map (GameController::ctor_0 @0x459c40), plus the eat/downed states.
            "0=Idle,1=Attack Right,2=Attack Left,3=Stab Right,4=Stab Left,5=Perforate,"
            "6=Punch Right,7=Punch Left,8=Block,9=Shield Attack,10=Shield Slam,11=Swirl,12=Slash,"
            "13=Slash Left,14=Slash Right,16=Mutilate,17=Ambush,18=Pierce Left,19=Pierce Right,"
            "20=Kick,21=Ranger Kick,22=Shoot,23=Shoot Fast,24=Charge Shoot,25=Salvo,26=Boomerang,"
            "27=Charge Boomerang,28=Beam,30=Fire Shock,31=Fire Burst,32=Water Shock,33=Water Burst,"
            "34=Healing Stream,36=Charge Bullet,37=Fireball,38=Firebolt,39=Firebolt,40=Firebolt,"
            "41=Waterbolt,42=Waterbolt,43=Waterball,44=Waterbolt,45=Water Salvo,46=Fire Salvo,"
            "48=Intercept,49=Teleport,50=Retreat,54=Smash,55=Salvo,57=Slam,58=Slam Back,"
            "59=Charge Slam,63=Swirl,64=Charge Mutilate,65=Slam Left,66=Slam Right,67=Jab,79=Sneak,"
            "86=Cyclone,88=Fire Explosion,95=Beam,96=Shuriken,97=Camouflage,99=Aim,100=Swiftness,"
            "101=Bulwark,102=War Frenzy,103=Mana Shield,106=Eating,110=Knocked Down",
            // CUBE_CATALOG_STRUCTURE_TYPE (zone structure record int +0x0); ids non-sequential
            // (gaps + repeats)
            "0=Statue,1=Door,2=Big Door,3=Window,4=Castle Window,5=Gate,6=Fire Trap,7=Spike Trap,"
            "8=Stomp Trap,9=Lever,10=Chest,12=Table,13=Table,14=Table,15=Stool,16=Stool,17=Stool,"
            "18=Bench,19=Bed,20=Bed Table,21=Market Stand 1,22=Market Stand 2,23=Market Stand 3,"
            "24=Barrel,25=Crate,26=Open Crate,27=Sack,28=Shelter,29=Cupboard,30=Desktop,31=Counter,"
            "32=Shelf 1,33=Shelf 2,34=Shelf 3,44=Corpse,45=Rune Stone,46=Artifact,47=Flower Box 1,"
            "48=Flower Box 2,49=Flower Box 3,50=Street Light,51=Fire Street Light,52=Fence 1,"
            "53=Fence 2,54=Fence 3,55=Fence 4,56=Vase 1,57=Vase 2,58=Vase 3,59=Vase 4,60=Vase 5,"
            "61=Vase 6,62=Vase 7,63=Vase 8,64=Vase 9,65=Campfire,66=Tent,67=Beach Umbrella,"
            "68=Beach Towel,69=Sleeping Mat,71=Furnace,72=Anvil,73=Spinning Wheel,74=Loom,"
            "75=Saw Bench,76=Workbench,77=Customization Bench",
            // CUBE_CATALOG_ENTITY_CATEGORY (Creature kind byte +0x60); kind 3 exists but is unlabeled
            "0=Player,1=Monster,5=Pet,6=NPC",
            // CUBE_CATALOG_CLASS (Creature byte +0x140)
            "1=Warrior,2=Ranger,3=Mage,4=Rogue",
            // CUBE_CATALOG_SPECIES (Creature int +0x64); complete table, ids 0..155 (95, 129 absent)
            "0=ElfMale,1=ElfFemale,2=HumanMale,3=HumanFemale,4=GoblinMale,5=GoblinFemale,"
            "6=Bullterrier,7=LizardmanMale,8=LizardmanFemale,9=DwarfMale,10=DwarfFemale,"
            "11=OrcMale,12=OrcFemale,13=FrogmanMale,14=FrogmanFemale,15=UndeadMale,"
            "16=UndeadFemale,17=Skeleton,18=OldMan,19=Collie,20=ShepherdDog,21=SkullBull,"
            "22=Alpaca,23=BrownAlpaca,24=Egg,25=Turtle,26=Terrier,27=ScottishTerrier,28=Wolf,"
            "29=Panther,30=Cat,31=BrownCat,32=WhiteCat,33=Pig,34=Sheep,35=Bunny,36=Porcupine,"
            "37=GreenSlime,38=PinkSlime,39=YellowSlime,40=BlueSlime,41=Frightener,42=SandHorror,"
            "43=Wizard,44=Bandit,45=Witch,46=Ogre,47=Rockling,48=Gnoll,49=PolarGnoll,50=Monkey,"
            "51=Gnobold,52=Insectoid,53=Hornet,54=InsectGuard,55=Crow,56=Chicken,57=Seagull,"
            "58=Parrot,59=Bat,60=Fly,61=Midge,62=Mosquito,63=PlainRunner,64=LeafRunner,"
            "65=SnowRunner,66=DesertRunner,67=Peacock,68=Frog,69=PlantCreature,70=RadishCreature,"
            "71=Onionling,72=DesertOnionling,73=Devourer,74=Duckbill,75=Crocodile,76=SpikeCreature,"
            "77=Anubis,78=Horus,79=Jester,80=Spectrino,81=Djinn,82=Minotaur,83=NomadMale,"
            "84=NomadFemale,85=Imp,86=Spitter,87=Mole,88=Biter,89=Koala,90=Squirrel,91=Raccoon,"
            "92=Owl,93=Penguin,94=Werewolf,96=Zombie,97=Vampire,98=Horse,99=Camel,100=Cow,"
            "101=Dragon,102=BarkBeetle,103=FireBeetle,104=SnoutBeetle,105=LemonBeetle,106=Crab,"
            "107=SeaCrab,108=Troll,109=DarkTroll,110=HellDemon,111=Golem,112=EmberGolem,"
            "113=SnowGolem,114=Yeti,115=Cyclops,116=Mammoth,117=Lich,118=RuneGiant,119=Saurian,"
            "120=Bush,121=SnowBush,122=SnowBerryBush,123=CottonPlant,124=Scrub,125=CobwebScrub,"
            "126=FireScrub,127=Ginseng,128=Cactus,130=ThornTree,131=GoldDeposit,132=IronDeposit,"
            "133=SilverDeposit,134=SandstoneDeposit,135=EmeraldDeposit,136=SapphireDeposit,"
            "137=RubyDeposit,138=DiamondDeposit,139=IceCrystalDeposit,140=Scarecrow,141=Aim,"
            "142=Dummy,143=Vase,144=Bomb,145=SapphireFish,146=LemonFish,147=Seahorse,148=Mermaid,"
            "149=Merman,150=Shark,151=Bumblebee,152=LanternFish,153=MawFish,154=Piranha,"
            "155=Blowfish",
            // CUBE_CATALOG_SKILL (skill-ranks array slot 0..10 -> skill name)
            "0=Pet Master,1=Riding,2=Climbing,3=Hang Gliding,4=Swimming,5=Sailing,"
            "6=Ability 1,7=Ability 2,8=Ability 3,9=Ability 4,10=Ability 5",
            // CUBE_CATALOG_ABILITY (ability id -> name): the castable/special abilities (the ids that
            // appear in the cooldown map), from the icon map + @tuning-var cross-check @0x459c40.
            "21=Ranger Kick,34=Healing Stream,48=Intercept,49=Teleport,50=Retreat,54=Smash,79=Sneak,"
            "86=Cyclone,88=Fire Explosion,96=Shuriken,97=Camouflage,99=Aim,100=Swiftness,"
            "101=Bulwark,102=War Frenzy,103=Mana Shield",
            // CUBE_CATALOG_CONSUMABLE_SUBTYPE (item subtype when type==Consumable/1)
            "0=Cookie,1=LifePotion,2=CactusPotion,3=ManaPotion,4=GinsengSoup,5=SnowBerryMash,"
            "6=MushroomSpit,7=Bomb,8=PineappleSlice,9=PumpkinMuffin",
            // CUBE_CATALOG_FOOD_SUBTYPE (item subtype when type==Food/20); only named overrides,
            // every other subtype is "Bait" (resolved by the loader, not stored here)
            "19=BubbleGum,22=VanillaCupcake,23=ChocolateCupcake,25=CinnamonRole,26=Waffle,"
            "27=Croissant,30=Candy,33=PumpkinMash,34=CottonCandy,35=Carrot,36=BlackberryMarmelade,"
            "37=GreenJelly,38=PinkJelly,39=YellowJelly,40=BlueJelly,50=BananaSplit,53=Popcorn,"
            "55=LicoriceCandy,56=CerealBar,57=SaltedCaramel,58=GingerTartlet,59=MangoJuice,"
            "60=FruitBasket,61=MelonIceCream,62=BloodOrangeJuice,63=MilkChocolateBar,"
            "64=MintChocolateBar,65=WhiteChocolateBar,66=CaramelChocolateBar,67=ChocolateCookie,"
            "74=SugarCandy,75=AppleRing,86=WaterIce,87=ChocolateDonut,88=Pancakes,90=StrawberryCake,"
            "91=ChocolateCake,92=Lollipop,93=Softice,98=CandiedApple,99=DateCookie,102=Bread,"
            "103=Curry,104=Lolly,105=LemonTart,106=StrawberryCocktail,151=BiscuitRole",
            // CUBE_CATALOG_SPECIAL_SUBTYPE (item subtype when type==Special/11)
            "0=Nugget,1=Log,2=Feather,3=Horn,4=Claw,5=Fiber,6=Cobweb,7=Hair,8=Crystal,9=Yarn,"
            "10=Cube,11=Capsule,12=Flask,13=Orb,14=Spirit,15=Mushroom,16=Pumpkin,17=Pineapple,"
            "18=RadishSlice,19=ShimmerMushroom,20=GinsengRoot,21=OnionSlice,22=Heartflower,"
            "23=PricklyPear,24=FrozenHeartflower,25=Soulflower,26=WaterFlask,27=SnowBerry",
            // CUBE_CATALOG_ACCESSORY_SUBTYPE (item subtype when type==Accessory/21)
            "0=Amulet 1,1=Amulet 2,2=JewelCase,3=Key,4=Medicine,5=Antivenom,6=BandAid,7=Crutch,"
            "8=Bandage,9=Salve",
            // CUBE_CATALOG_VEHICLE_SUBTYPE (item subtype when type==Vehicle/23)
            "0=HangGlider,1=Boat",
            // CUBE_CATALOG_SOUND (built-in sfx id -> wav base name, for audio.playSound)
            "0=hit,1=blade1,2=blade2,3=long-blade1,4=long-blade2,5=hit1,6=hit2,7=punch1,8=punch2,"
            "9=hit-arrow,10=hit-arrow-critical,11=smash1,12=slam-ground,13=smash-hit2,14=smash-jump,"
            "15=swing,16=shield-swing,17=swing-slow,18=swing-slow2,19=arrow-destroy,20=blade1b,"
            "21=punch2b,22=salvo2,23=sword-hit03,24=block,25=shield-slam,26=roll,27=destroy2,28=cry,"
            "29=levelup2,30=missioncomplete,31=water-splash01,32=step2,33=step-water,34=step-water3,"
            "35=channel2,36=channel2b,37=channel-hit,38=fireball,39=fire-hit,40=magic02,"
            "41=watersplash,42=watersplash-hit,43=lich-scream,44=drink2,45=pickup,46=disenchant2,"
            "47=upgrade2,48=swirl,49=human-voice01,50=human-voice02,51=gate,52=spike-trap,"
            "53=fire-trap,54=lever,55=charge2,56=magic02b,57=drop,58=drop-coin,59=drop-item,"
            "60=male-groan,61=female-groan,62=male-groan2,63=female-groan2,64=goblin-male-groan,"
            "65=goblin-female-groan,66=lizard-male-groan,67=lizard-female-groan,68=dwarf-male-groan,"
            "69=dwarf-female-groan,70=orc-male-groan,71=orc-female-groan,72=undead-male-groan,"
            "73=undead-female-groan,74=frogman-male-groan,75=frogman-female-groan,76=monster-groan,"
            "77=troll-groan,78=mole-groan,79=slime-groan,80=zombie-groan,81=Explosion,82=punch2c,"
            "83=menu-open2,84=menu-close2,85=menu-select,86=menu-tab,87=menu-grab-item,"
            "88=menu-drop-item,89=craft,90=craft-proc,91=absorb,92=manashield,93=bulwark,94=bird1,"
            "95=bird2,96=bird3,97=cricket1,98=cricket2,99=owl1,100=owl2"
        };

        typedef std::vector<std::pair<int32_t, std::string>> NameList;

        struct CatalogTables
        {
            NameList lists[CUBE_CATALOG_COUNT];
        };

        NameList parse(const char* data)
        {
            NameList result;
            const char* cursor = data;
            while (*cursor)
            {
                const int32_t id = static_cast<int32_t>(std::atoi(cursor));
                while (*cursor && *cursor != '=')
                    ++cursor;

                if (*cursor == '=')
                    ++cursor;

                std::string name;
                while (*cursor && *cursor != ',')
                    name.push_back(*cursor++);

                if (*cursor == ',')
                    ++cursor;

                result.push_back(std::make_pair(id, name));
            }
            return result;
        }

        CatalogTables buildTables()
        {
            CatalogTables tables;
            for (int32_t i = 0; i < CUBE_CATALOG_COUNT; ++i)
                tables.lists[i] = parse(kCatalogData[i]);

            return tables;
        }

        const CatalogTables& tables()
        {
            static const CatalogTables g_tables = buildTables();
            return g_tables;
        }

    }

    const char* catalogName(int32_t catalog, int32_t id)
    {
        if (catalog < 0 || catalog >= CUBE_CATALOG_COUNT)
            return nullptr;

        const NameList& list = tables().lists[catalog];
        for (const std::pair<int32_t, std::string>& entry : list)
        {
            if (entry.first == id)
                return entry.second.c_str();
        }

        return nullptr;
    }

    int32_t catalogList(int32_t catalog, CubeNamedValue* out, int32_t maxCount)
    {
        if (!out || maxCount <= 0 || catalog < 0 || catalog >= CUBE_CATALOG_COUNT)
            return 0;

        const NameList& list = tables().lists[catalog];
        int32_t count = 0;

        for (const std::pair<int32_t, std::string>& entry : list)
        {
            if (count >= maxCount)
                break;

            out[count].id = entry.first;
            out[count].name = entry.second.c_str();
            ++count;
        }
        return count;
    }

    void catalogNameOr(int32_t catalog, int32_t id, char* out, int32_t size, const char* prefix)
    {
        const char* name = catalogName(catalog, id);
        if (name)
            std::snprintf(out, size, "%s", name);
        else
            std::snprintf(out, size, "%s #%d", prefix, id);
    }
}