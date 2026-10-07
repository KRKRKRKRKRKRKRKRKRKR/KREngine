#include "KR/Core/Assets.h"

#ifndef KR_DEBUG
#include <Windows.h>
#endif

namespace KR
{
    std::filesystem::path GetAssetDirectory(const std::filesystem::path& name)
    {
#ifdef KR_DEBUG
        return std::filesystem::path(KR_ASSETS_SOURCE_DIR) / name;
#else
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        return std::filesystem::path(exePath).parent_path() / name;
#endif
    }
}
