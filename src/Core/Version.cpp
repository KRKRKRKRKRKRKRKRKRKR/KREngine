#include "KR/Core/Version.h"

namespace KR
{
    Version GetVersion()
    {
        // CMake が、コンパイルのときに、KR_VERSION_* を定義する(エンジンの CMakeLists.txt の project の VERSION)
        return { KR_VERSION_MAJOR, KR_VERSION_MINOR, KR_VERSION_PATCH };
    }

    std::string GetVersionString()
    {
        const Version version = GetVersion();
        return std::to_string(version.major) + "." + std::to_string(version.minor) + "." + std::to_string(version.patch);
    }
}
