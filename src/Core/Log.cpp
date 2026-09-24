#include "TenFiftyEight/Core/Log.h"

#include <Windows.h>

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>

namespace TenFiftyEight::Log
{
    namespace
    {
        LogLevel s_MinLevel = LogLevel::Trace;
        std::mutex s_Mutex;

        struct LevelStyle
        {
            std::string_view name;
            std::string_view color; // ANSI エスケープシーケンス
        };

        LevelStyle GetStyle(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Trace: return { "Trace", "\x1b[90m" };    // 灰
            case LogLevel::Info:  return { "Info",  "\x1b[37m" };    // 白
            case LogLevel::Warn:  return { "Warn",  "\x1b[33m" };    // 黄
            case LogLevel::Error: return { "Error", "\x1b[31m" };    // 赤
            case LogLevel::Fatal: return { "Fatal", "\x1b[97;41m" }; // 白文字・赤背景
            }
            return { "?", "" };
        }

        std::string_view FileNameOnly(std::string_view path)
        {
            const size_t pos = path.find_last_of("/\\");
            return pos == std::string_view::npos ? path : path.substr(pos + 1);
        }

        // OutputDebugStringA は UTF-8 を正しく扱えないため、UTF-16 に変換して W 版を使う
        void OutputToDebugger(const std::string& utf8)
        {
            const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
            std::wstring wide(length, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
            OutputDebugStringW(wide.c_str());
        }
    }

    //ログの初期化処理
    void Init()
    {

        SetConsoleOutputCP(CP_UTF8);

        // コンソールで ANSI エスケープシーケンス(色)を有効にする
        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(console, &mode))
        {
            SetConsoleMode(console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    void SetMinLevel(LogLevel level)
    {
        s_MinLevel = level;
    }

    void Write(LogLevel level, std::string_view message, const std::source_location& location)
    {
        if (level < s_MinLevel)
        {
            return;
        }

        SYSTEMTIME time;
        GetLocalTime(&time);

        const LevelStyle style = GetStyle(level);

        std::string line = std::format("[{:02}:{:02}:{:02}] [{}] {}",
            time.wHour, time.wMinute, time.wSecond, style.name, message);

        if (level >= LogLevel::Warn)
        {
            line += std::format(" ({}:{})", FileNameOnly(location.file_name()), location.line());
        }
        {
            std::scoped_lock lock(s_Mutex);
            std::fputs(std::format("{}{}\x1b[0m\n", style.color, line).c_str(), stdout);
            OutputToDebugger(line + '\n');
        }

        if (level == LogLevel::Fatal)
        {
            std::fflush(stdout);
            if (IsDebuggerPresent())
            {
                __debugbreak();
            }
            std::abort();
        }
    }
}
