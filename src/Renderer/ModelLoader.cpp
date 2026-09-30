#include "TenFiftyEight/Renderer/ModelLoader.h"

#include "TenFiftyEight/Core/Log.h"

#include <Windows.h>

#include <fstream>
#include <sstream>
#include <string>

namespace TenFiftyEight
{
    namespace
    {
        // Debug: 編集中の元フォルダー / Release: exe の横にコピーされたもの(シェーダーと同じ方式)
        std::filesystem::path GetModelDirectory()
        {
#ifdef TFE_DEBUG
            return std::filesystem::path(TFE_MODEL_SOURCE_DIR);
#else
            wchar_t exePath[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            return std::filesystem::path(exePath).parent_path() / L"Models";
#endif
        }
    }

    // 3-2: ファイルを開いて1行ずつ読み、行の種類ごとに振り分ける(今は数えるだけ)
    MeshData LoadObj(const std::filesystem::path& relativePath)
    {
        const std::filesystem::path fullPath = GetModelDirectory() / relativePath;

        std::ifstream file(fullPath);
        if (!file.is_open())
        {
            TFE_LOG_ERROR("LoadObj: ファイルを開けません: {}", fullPath.string());
            return {};
        }

        size_t positionLines = 0; // v
        size_t normalLines = 0;   // vn
        size_t faceLines = 0;     // f
        size_t materialUses = 0;  // usemtl
        size_t otherLines = 0;    // 上記以外(コメント・空行・vt・o・g など。今は使わない)

        std::string line;
        while (std::getline(file, line))
        {
            // 行の先頭の単語(v / vn / f …)を取り出す。空行なら空文字になる
            std::istringstream stream(line);
            std::string keyword;
            stream >> keyword;

            if (keyword == "v")
            {
                ++positionLines;
            }
            else if (keyword == "vn")
            {
                ++normalLines;
            }
            else if (keyword == "f")
            {
                ++faceLines;
            }
            else if (keyword == "usemtl")
            {
                ++materialUses;
            }
            else
            {
                ++otherLines;
            }
        }

        TFE_LOG_INFO("LoadObj: {} v={} vn={} f={} usemtl={} その他={}",
            relativePath.string(), positionLines, normalLines, faceLines, materialUses, otherLines);

        return {};
    }
}
