#include "KR/Behavior/LiveGraph.h"

#include "KR/Behavior/GraphFile.h"

#include <Windows.h>

#include <cwctype>
#include <format>
#include <string>

namespace KR::Behavior
{
    namespace
    {
        // Enemies フォルダーの場所から、短い印(16 桁の 16 進)を作る
        // 同じフォルダーでも書き方が違う(大文字 / 小文字・.. を含む・/ と \)と、印が変わってしまうので、先にそろえる
        std::wstring MakeFolderKey(const std::filesystem::path& folder)
        {
            std::error_code error;
            std::filesystem::path normalized = std::filesystem::weakly_canonical(folder, error);
            if (error)
            {
                normalized = folder.lexically_normal();
            }
            std::wstring text = normalized.make_preferred().wstring();
            while (!text.empty() && text.back() == L'\\')
            {
                text.pop_back(); // 最後の \ の有る無しで、印が変わらないように
            }

            // FNV-1a(64 ビット):文字を 1 つずつ混ぜていく、簡単な数え方。Windows の場所は、大文字と小文字を区別しないので、小文字にしてから混ぜる
            uint64_t hash = 14695981039346656037ull;
            for (const wchar_t c : text)
            {
                hash ^= static_cast<uint64_t>(std::towlower(c));
                hash *= 1099511628211ull;
            }
            return std::format(L"{:016x}", hash);
        }

        std::filesystem::path GetLiveRoot()
        {
            wchar_t temp[MAX_PATH + 1] = {};
            const DWORD length = GetTempPathW(MAX_PATH + 1, temp);
            const std::filesystem::path tempFolder = (length == 0) ? std::filesystem::temp_directory_path() : std::filesystem::path(temp);
            return tempFolder / L"KREngineLive";
        }
    }

    std::filesystem::path GetLiveGraphFolder(const std::filesystem::path& enemiesFolder)
    {
        return GetLiveRoot() / MakeFolderKey(enemiesFolder);
    }

    std::filesystem::path GetLiveGraphFile(const std::filesystem::path& graphFile)
    {
        return GetLiveGraphFolder(graphFile.parent_path()) / graphFile.filename();
    }

    bool WriteLiveGraph(const Graph& graph, const std::filesystem::path& graphFile)
    {
        // SaveGraph は、フォルダーを作り、一時ファイルに書いてから置き換える(書きかけを読ませない)
        return SaveGraph(graph, GetLiveGraphFile(graphFile));
    }

    void DeleteLiveGraphFile(const std::filesystem::path& graphFile)
    {
        std::error_code error;
        std::filesystem::remove(GetLiveGraphFile(graphFile), error); // 無ければ、何もしない
    }

    void DeleteAllLiveGraphFiles(const std::filesystem::path& enemiesFolder)
    {
        std::error_code error;
        std::filesystem::remove_all(GetLiveGraphFolder(enemiesFolder), error);
    }
}
