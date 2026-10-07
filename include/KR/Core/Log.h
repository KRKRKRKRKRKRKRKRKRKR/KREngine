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
#define KR_LOG_IMPL(level, ...) \
    ::KR::Log::Print(::KR::LogLevel::level, std::source_location::current(), __VA_ARGS__)

#ifdef KR_DEBUG
    #define KR_LOG_TRACE(...) KR_LOG_IMPL(Trace, __VA_ARGS__)
    #define KR_LOG_INFO(...)  KR_LOG_IMPL(Info, __VA_ARGS__)
#else
    #define KR_LOG_TRACE(...) ((void)0)
    #define KR_LOG_INFO(...)  ((void)0)
#endif

#define KR_LOG_WARN(...)  KR_LOG_IMPL(Warn, __VA_ARGS__)
#define KR_LOG_ERROR(...) KR_LOG_IMPL(Error, __VA_ARGS__)
#define KR_LOG_FATAL(...) KR_LOG_IMPL(Fatal, __VA_ARGS__)
