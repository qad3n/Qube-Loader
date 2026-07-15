#include "api/bridge.h"
#include "modloader/core/modconfig.h"
#include "modloader/core/owner_name.h"
#include "cube_sdk.h"

#include <cstring>
#include <string>

namespace modloader::api
{
    namespace
    {
        int32_t CUBE_CALL apiConfigGetInt(const CubeApi* api, const char* key, int32_t fallback)
        {
            if (!api || !key)
                return fallback;
            const int32_t value = modconfig::getInt(ownerStem(api), key, fallback);
            LOGC(Trace, kApiCategory, "'%s' config.getInt(%s) -> %d", modName(api), key, value);
            return value;
        }

        double CUBE_CALL apiConfigGetFloat(const CubeApi* api, const char* key, double fallback)
        {
            if (!api || !key)
                return fallback;
            const double value = modconfig::getFloat(ownerStem(api), key, fallback);
            LOGC(Trace, kApiCategory, "'%s' config.getFloat(%s) -> %.3f", modName(api), key, value);
            return value;
        }

        int32_t CUBE_CALL apiConfigGetBool(const CubeApi* api, const char* key, int32_t fallback)
        {
            if (!api || !key)
                return fallback;
            const bool value = modconfig::getBool(ownerStem(api), key, fallback != 0);
            LOGC(Trace, kApiCategory, "'%s' config.getBool(%s) -> %d", modName(api), key, value);
            return okInt(value);
        }

        int32_t CUBE_CALL apiConfigGetString(const CubeApi* api, const char* key, const char* fallback,
                                             char* out, int32_t size)
        {
            if (!out || size <= 0)
                return 0;
            const std::string value = (api && key)
                ? modconfig::getString(ownerStem(api), key, fallback ? fallback : "")
                : (fallback ? fallback : "");
            const int32_t copyLen = value.size() < static_cast<size_t>(size - 1)
                ? static_cast<int32_t>(value.size())
                : size - 1;
            std::memcpy(out, value.data(), static_cast<size_t>(copyLen));
            out[copyLen] = '\0';
            LOGC(Trace, kApiCategory, "'%s' config.getString(%s) -> \"%s\"", modName(api), key ? key : "(null)", out);
            return copyLen;
        }

        int32_t CUBE_CALL apiConfigSetInt(const CubeApi* api, const char* key, int32_t value)
        {
            if (!api || !key)
                return 0;
            const bool ok = modconfig::setInt(ownerStem(api), key, value);
            LOGC(Debug, kApiCategory, "'%s' config.setInt(%s, %d) -> %s", modName(api), key, value, ok ? "ok" : "fail");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiConfigSetFloat(const CubeApi* api, const char* key, double value)
        {
            if (!api || !key)
                return 0;
            const bool ok = modconfig::setFloat(ownerStem(api), key, value);
            LOGC(Debug, kApiCategory, "'%s' config.setFloat(%s, %.3f) -> %s", modName(api), key, value, ok ? "ok" : "fail");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiConfigSetBool(const CubeApi* api, const char* key, int32_t value)
        {
            if (!api || !key)
                return 0;
            const bool ok = modconfig::setBool(ownerStem(api), key, value != 0);
            LOGC(Debug, kApiCategory, "'%s' config.setBool(%s, %d) -> %s", modName(api), key, value != 0, ok ? "ok" : "fail");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiConfigSetString(const CubeApi* api, const char* key, const char* value)
        {
            if (!api || !key)
                return 0;
            const bool ok = modconfig::setString(ownerStem(api), key, value ? value : "");
            LOGC(Debug, kApiCategory, "'%s' config.setString(%s, \"%s\") -> %s", modName(api), key, value ? value : "", ok ? "ok" : "fail");
            return okInt(ok);
        }
    }

    void fillConfig(CubeApi& api)
    {
        api.config.getInt = &apiConfigGetInt;
        api.config.getFloat = &apiConfigGetFloat;
        api.config.getBool = &apiConfigGetBool;
        api.config.getString = &apiConfigGetString;
        api.config.setInt = &apiConfigSetInt;
        api.config.setFloat = &apiConfigSetFloat;
        api.config.setBool = &apiConfigSetBool;
        api.config.setString = &apiConfigSetString;
    }
}
