#include "api/bridge.h"
#include "game/pet.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        int32_t CUBE_CALL apiPetGet(const CubeApi* api, CubePet* out)
        {
            return bridgeGet<CubePet>(api, out, &game::readPet, "pet.get", "none", &CubePet::address);
        }
    }

    void fillPet(CubeApi& api)
    {
        api.pet.get = &apiPetGet;
    }
}
