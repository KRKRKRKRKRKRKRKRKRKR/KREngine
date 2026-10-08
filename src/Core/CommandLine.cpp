#include "KR/Core/CommandLine.h"

#include <Windows.h>
#include <shellapi.h>

namespace KR
{
    std::vector<std::filesystem::path> GetCommandLineArguments()
    {
        std::vector<std::filesystem::path> result;

        // ワイド文字(UTF-16)の引数を、取り出す。ANSI の argv だと、日本語のパスが、化ける
        int count = 0;
        LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        if (!arguments)
        {
            return result;
        }
        for (int i = 0; i < count; ++i)
        {
            result.emplace_back(arguments[i]);
        }
        LocalFree(arguments);
        return result;
    }
}
