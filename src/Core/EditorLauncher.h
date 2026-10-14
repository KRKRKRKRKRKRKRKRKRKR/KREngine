#pragma once

#include <Windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace KR
{
    // ツール(別の exe)を起動して、動いているかを見張る係(エンジン内部専用。Debug のゲームの F1 で使う)
    // 1 つのツールにつき 1 つ持つ。起動したツールは、ゲームが終わっても、閉じない(編集の途中かもしれないため)
    class EditorLauncher
    {
    public:
        EditorLauncher() = default;
        ~EditorLauncher();

        EditorLauncher(const EditorLauncher&) = delete;
        EditorLauncher& operator=(const EditorLauncher&) = delete;

        // ツールが動いていなければ起動し、動いていれば、その窓を前に出す(2 つ目は起動しない)
        // 起動できた・前に出せたら true。exe が無い・起動に失敗したら、警告をログに出して false
        bool OpenOrFocus(const std::filesystem::path& exe, const std::vector<std::wstring>& arguments);

        // ツールが、まだ動いているか(閉じられていたら、プロセスの札を返して false)。毎フレーム呼んでも軽い(待たない)
        bool IsRunning();

    private:
        HANDLE m_Process = nullptr; // 起動したツールのプロセスの札(終わったかを調べるため)
        DWORD m_ProcessId = 0;      // 窓を探すため
    };
}
