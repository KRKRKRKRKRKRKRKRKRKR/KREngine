#include "KR/Core/Assets.h"

#ifndef KR_DEBUG
#include <Windows.h>
#endif

namespace KR
{
#ifdef KR_DEBUG
    namespace
    {
        // CMake が埋め込む場所(KR_*_SOURCE_DIR)は、/utf-8 で、UTF-8 のバイト列になる。
        // path(const char*) は、Windows では、標準のコードページ(日本語 Windows では CP932)として読むので、
        // 日本語のフォルダー名が、変換に失敗して、例外(std::system_error)になる。char8_t の列として渡して、UTF-8 として読ませる
        std::filesystem::path Utf8Path(const char* utf8)
        {
            return std::filesystem::path(reinterpret_cast<const char8_t*>(utf8));
        }
    }
#endif

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
        return Utf8Path(KR_ASSETS_SOURCE_DIR) / name;
#else
        return GetExeDirectory() / name;
#endif
    }

    std::filesystem::path GetEngineShaderDirectory()
    {
#ifdef KR_DEBUG
        return Utf8Path(KR_SHADER_SOURCE_DIR);
#else
        return GetExeDirectory() / "Shaders";
#endif
    }
}
