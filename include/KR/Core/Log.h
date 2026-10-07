#pragma once

#include <format>
#include <source_location>
#include <string_view>

namespace KR
{
    enum class LogLevel
    {
        Trace,
        Info,
        Warn,
        Error,
        Fatal,
    };

    namespace Log
    {
        void Init();
        void SetMinLevel(LogLevel level);

        // Fatal の場合は出力後にプログラムを停止する
        void Write(LogLevel level, std::string_view message, const std::source_location& location);

        template <typename... Args>
        void Print(LogLevel level, const std::source_location& location,
                   std::format_string<Args...> format, Args&&... args)
        {
            Write(level, std::format(format, std::forward<Args>(args)...), location);
        }
    }
}

// マクロにする理由: 呼び出し元の source_location を自動で渡すため、
// および Release で Trace / Info をコードごと消すため
#define TFE_LOG_IMPL(level, ...) \
    ::KR::Log::Print(::KR::LogLevel::level, std::source_location::current(), __VA_ARGS__)

#ifdef TFE_DEBUG
    #define TFE_LOG_TRACE(...) TFE_LOG_IMPL(Trace, __VA_ARGS__)
    #define TFE_LOG_INFO(...)  TFE_LOG_IMPL(Info, __VA_ARGS__)
#else
    #define TFE_LOG_TRACE(...) ((void)0)
    #define TFE_LOG_INFO(...)  ((void)0)
#endif

#define TFE_LOG_WARN(...)  TFE_LOG_IMPL(Warn, __VA_ARGS__)
#define TFE_LOG_ERROR(...) TFE_LOG_IMPL(Error, __VA_ARGS__)
#define TFE_LOG_FATAL(...) TFE_LOG_IMPL(Fatal, __VA_ARGS__)
