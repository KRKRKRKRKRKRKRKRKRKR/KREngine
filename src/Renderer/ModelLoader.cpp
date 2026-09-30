#include "TenFiftyEight/Renderer/ModelLoader.h"

#include "TenFiftyEight/Core/Log.h"

#include <Windows.h>

#include <DirectXMath.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace DirectX;

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

    // 3-2: ファイルを開いて1行ずつ読み、行の種類ごとに振り分ける
    // 3-3: v の行を読んで、位置を部品棚(positions)に並べる
    MeshData LoadObj(const std::filesystem::path& relativePath)
    {
        const std::filesystem::path fullPath = GetModelDirectory() / relativePath;

        std::ifstream file(fullPath);
        if (!file.is_open())
        {
            TFE_LOG_ERROR("LoadObj: ファイルを開けません: {}", fullPath.string());
            return {};
        }

        std::vector<XMFLOAT3> positions; // 部品棚: v の位置。OBJ の番号は 1 始まりだが、ここは 0 始まり

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
                float x, y, z;
                if (!(stream >> x >> y >> z))
                {
                    // 数字が3つ読めない行は不良品。棚に並べずに飛ばす
                    TFE_LOG_WARN("LoadObj: {} の v の行を読めません: {}", relativePath.string(), line);
                    continue;
                }

                // OBJ は右手系、このエンジンは左手系。Z の符号を反転して、このエンジンの向きに直す
                // (面の向きが逆になる分は、f の処理で三角形の番号の順を入れ替えて直す)
                positions.push_back({ x, y, -z });
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
            relativePath.string(), positions.size(), normalLines, faceLines, materialUses, otherLines);
        if (!positions.empty())
        {
            // 3-3 動作確認用(確認が終わったら消す)
            TFE_LOG_INFO("LoadObj: 最初の位置 ({}, {}, {})", positions[0].x, positions[0].y, positions[0].z);
        }

        return {};
    }
}
