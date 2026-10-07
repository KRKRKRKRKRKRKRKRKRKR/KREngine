#include "KR/Core/Assets.h"

#ifndef KR_DEBUG
#include <Windows.h>
#endif

namespace KR
{
#ifndef KR_DEBUG
    namespace
    {
        // exe のあるフォルダー
        std::filesystem::path GetExeDirectory()
        {
            wchar_t exePath[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            return std::filesystem::path(exePath).parent_path();
        }
    }
#endif

    std::filesystem::path GetAssetDirectory(const std::filesystem::path& name)
    {
#ifdef KR_DEBUG
        return std::filesystem::path(KR_ASSETS_SOURCE_DIR) / name;
#else
        return GetExeDirectory() / name;
#endif
    }

    std::filesystem::path GetEngineShaderDirectory()
    {
#ifdef KR_DEBUG
        return std::filesystem::path(KR_SHADER_SOURCE_DIR);
#else
        return GetExeDirectory() / "Shaders";
#endif
    }
}
