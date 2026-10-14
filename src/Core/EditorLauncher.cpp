#include "Core/EditorLauncher.h"

#include "KR/Core/Log.h"

namespace KR
{
    namespace
    {
        std::string ToUtf8(const std::filesystem::path& path)
        {
            const std::u8string utf8 = path.u8string();
            return std::string(utf8.begin(), utf8.end());
        }

        // コマンドラインの 1 つの引数を、"" で囲む(空白を含むフォルダーのため)
        // Windows の決まりで、" の直前の \ は、" を文字にする印になる。フォルダーの最後の \ を、2 つにして防ぐ
        std::wstring Quote(const std::wstring& argument)
        {
            std::wstring quoted = L"\"" + argument;
            if (!argument.empty() && argument.back() == L'\\')
            {
                quoted += L'\\';
            }
            return quoted + L"\"";
        }

        // プロセス id から、そのプロセスの、見えている一番上の窓を探す
        struct FindWindowData
        {
            DWORD processId = 0;
            HWND found = nullptr;
        };

        BOOL CALLBACK FindWindowOfProcess(HWND window, LPARAM lParam)
        {
            auto* data = reinterpret_cast<FindWindowData*>(lParam);
            DWORD processId = 0;
            GetWindowThreadProcessId(window, &processId);
            if (processId == data->processId && IsWindowVisible(window) && GetWindow(window, GW_OWNER) == nullptr)
            {
                data->found = window;
                return FALSE; // 見つかったので、探すのをやめる
            }
            return TRUE;
        }
    }

    EditorLauncher::~EditorLauncher()
    {
        // 札を返すだけ。ツールは閉じない(保存していない編集が、消えないように)
        if (m_Process)
        {
            CloseHandle(m_Process);
        }
    }

    bool EditorLauncher::OpenOrFocus(const std::filesystem::path& exe, const std::vector<std::wstring>& arguments)
    {
        if (IsRunning())
        {
            // もう動いている → 窓を前に出す(最小化していたら、元に戻す)
            FindWindowData data;
            data.processId = m_ProcessId;
            EnumWindows(&FindWindowOfProcess, reinterpret_cast<LPARAM>(&data));
            if (data.found)
            {
                if (IsIconic(data.found))
                {
                    ShowWindow(data.found, SW_RESTORE);
                }
                SetForegroundWindow(data.found);
            }
            return true;
        }

        std::error_code error;
        if (!std::filesystem::is_regular_file(exe, error))
        {
            KR_LOG_WARN("ツールがありません: {}(ゲームの CMake で KR_BUILD_TOOLS を ON にして、Debug でビルドすると、exe の横にコピーされます)", ToUtf8(exe));
            return false;
        }

        // コマンドライン:"ツール.exe" 引数 引数 …(CreateProcessW は、書き換えられる文字列を求める)
        std::wstring commandLine = Quote(exe.wstring());
        for (const std::wstring& argument : arguments)
        {
            commandLine += L" " + Quote(argument);
        }

        STARTUPINFOW startup = {};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process = {};
        const std::wstring workingDirectory = exe.parent_path().wstring(); // ツールは、自分の横の Shaders などを読む
        if (!CreateProcessW(exe.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, workingDirectory.c_str(), &startup, &process))
        {
            KR_LOG_WARN("ツールを起動できません(エラー {}): {}", GetLastError(), ToUtf8(exe));
            return false;
        }

        CloseHandle(process.hThread); // スレッドの札は使わない
        m_Process = process.hProcess;
        m_ProcessId = process.dwProcessId;
        KR_LOG_INFO("ツールを起動しました: {}", ToUtf8(exe.filename()));
        return true;
    }

    bool EditorLauncher::IsRunning()
    {
        if (!m_Process)
        {
            return false;
        }
        // 待ち時間 0 で調べる:まだ動いていれば WAIT_TIMEOUT
        if (WaitForSingleObject(m_Process, 0) == WAIT_TIMEOUT)
        {
            return true;
        }
        CloseHandle(m_Process);
        m_Process = nullptr;
        m_ProcessId = 0;
        KR_LOG_INFO("ツールが閉じられました");
        return false;
    }
}
