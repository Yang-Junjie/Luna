#include "ScriptHostBridgeInternal.h"

namespace luna {

void initializeScriptHostApiBridge(LunaScriptHostApi& host_api)
{
    registerScriptSceneHostApi(host_api);
    registerScriptEntityHostApi(host_api);
    registerScriptInputHostApi(host_api);
}

} // namespace luna
