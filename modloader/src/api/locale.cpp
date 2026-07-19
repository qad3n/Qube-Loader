#include "api/bridge.h"
#include "modloader/core/modlocale.h"
#include "modloader/core/owner_name.h"
#include "cube_sdk.h"

#include <cstring>
#include <string>

namespace modloader::api
{
    namespace
    {
        // Copies text into out (capped, always null-terminated) and returns the copied length.
        int32_t copyOut(const std::string& text, char* out, int32_t size)
        {
            const int32_t copyLen = text.size() < static_cast<size_t>(size - 1)
                ? static_cast<int32_t>(text.size())
                : size - 1;
            std::memcpy(out, text.data(), static_cast<size_t>(copyLen));
            out[copyLen] = '\0';
            return copyLen;
        }

        int32_t CUBE_CALL apiLocaleTranslate(const CubeApi* api, const char* key, const char* fallback,
                                             char* out, int32_t size)
        {
            if (!out || size <= 0)
                return 0;
            // A null fallback falls back to the key text, so an untranslated string still renders.
            const std::string fallbackText = fallback ? fallback : (key ? key : "");
            const std::string value = (api && key)
                ? modlocale::translate(ownerStem(api), key, fallbackText)
                : fallbackText;
            const int32_t copyLen = copyOut(value, out, size);
            LOGC(Trace, kApiCategory, "'%s' locale.translate(%s) -> \"%s\"", modName(api), key ? key : "(null)", out);
            return copyLen;
        }

        int32_t CUBE_CALL apiLocaleSetLocale(const CubeApi* api, const char* locale)
        {
            if (!api || !locale)
                return 0;
            modlocale::setLocale(ownerStem(api), locale);
            LOGC(Debug, kApiCategory, "'%s' locale.setLocale(%s)", modName(api), locale);
            return 1;
        }

        int32_t CUBE_CALL apiLocaleGetLocale(const CubeApi* api, char* out, int32_t size)
        {
            if (!out || size <= 0)
                return 0;
            const std::string value = api ? modlocale::getLocale(ownerStem(api)) : std::string();
            const int32_t copyLen = copyOut(value, out, size);
            LOGC(Trace, kApiCategory, "'%s' locale.getLocale -> \"%s\"", modName(api), out);
            return copyLen;
        }
    }

    void fillLocale(CubeApi& api)
    {
        api.locale.translate = &apiLocaleTranslate;
        api.locale.setLocale = &apiLocaleSetLocale;
        api.locale.getLocale = &apiLocaleGetLocale;
    }
}
