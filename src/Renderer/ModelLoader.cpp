#include "TenFiftyEight/Renderer/ModelLoader.h"

#include "TenFiftyEight/Core/Log.h"

#include <Windows.h>

#include <DirectXMath.h>

#include <charconv>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
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

        // 台帳のキー: 「位置の番号」と「材質の番号」の組。両方同じなら同じ頂点とみなす
        using VertexKey = std::pair<size_t, size_t>;

        // 3-5-1: .mtl(色見本帳)を読んで、「材質の名前 → 色」の表を作る。今使うのは newmtl と Kd だけ
        // 開けなければ警告を出して空の表を返す(その場合は全部白で表示される)
        std::map<std::string, XMFLOAT4> LoadMtl(const std::filesystem::path& mtlPath)
        {
            std::ifstream file(mtlPath);
            if (!file.is_open())
            {
                TFE_LOG_WARN("LoadObj: .mtl を開けません: {}", mtlPath.string());
                return {};
            }

            std::map<std::string, XMFLOAT4> colors;
            std::string currentName; // 今書いている塗料の名前(newmtl で更新)

            std::string line;
            while (std::getline(file, line))
            {
                std::istringstream stream(line);
                std::string keyword;
                stream >> keyword;

                if (keyword == "newmtl")
                {
                    stream >> currentName;
                }
                else if (keyword == "Kd")
                {
                    float r, g, b;
                    if (currentName.empty() || !(stream >> r >> g >> b))
                    {
                        // 名前がまだない、または数字が3つ読めない行は無視する
                        TFE_LOG_WARN("LoadObj: {} の Kd の行を読めません: {}", mtlPath.string(), line);
                        continue;
                    }
                    colors[currentName] = { r, g, b, 1.0f };
                }
            }
            return colors;
        }

        // 3-4-1: 面の1要素("5" "5/2" "5//1" "5/2/1" など)から、位置の番号だけを取り出して、棚の番号(0 始まり)にする
        // 成功したら true。数字でない・0・棚の範囲外なら false(設計図の不良)
        bool ParseFacePositionIndex(const std::string& element, size_t positionCount, size_t& outIndex)
        {
            // 最初の「/」の手前までが位置の番号(「/」がなければ全体)
            const std::string_view number = std::string_view(element).substr(0, element.find('/'));

            int value = 0;
            const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), value);
            if (error != std::errc() || end != number.data() + number.size() || value == 0)
            {
                return false;
            }

            // 正の数は 1 始まりなので 1 を引く。負の数は「棚の最後から数える」(-1 = 最後の部品)
            const long long index = value > 0 ? value - 1 : static_cast<long long>(positionCount) + value;
            if (index < 0 || index >= static_cast<long long>(positionCount))
            {
                return false;
            }

            outIndex = static_cast<size_t>(index);
            return true;
        }
    }

    // 3-2: ファイルを開いて1行ずつ読み、行の種類ごとに振り分ける
    // 3-3: v の行を読んで、位置を部品棚(positions)に並べる
    // 3-4-1: f の各要素から棚の部品を取り出す
    // 3-4-2: 同じ頂点を使い回して、頂点リストを作る
    // 3-4-3: 三角形に分けて、番号リストを作る(表裏も直す)
    // 3-5-1: mtllib の .mtl を読んで、色見本帳を作る
    // 3-5-2: usemtl で材質を切り替えて、頂点に色を付ける
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

        MeshData mesh;                                     // 完成品(頂点リストと番号リスト)
        std::map<VertexKey, uint32_t> vertexLookup;        // 台帳: 作った頂点のキー → mesh.vertices の何番目か
        // 色の一覧: 0 番は白(材質がないときの色)、1 番以降は mtllib で読んだ材質
        std::vector<XMFLOAT4> materialPalette = { { 1.0f, 1.0f, 1.0f, 1.0f } };
        std::map<std::string, size_t> materialIndexByName; // 材質の名前 → materialPalette の何番か
        size_t currentMaterial = 0;                        // 今の材質の番号(usemtl で切り替わる)

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
            else if (keyword == "f")
            {
                // 面の要素を1つずつ読んで、棚の番号にする。先に全部確かめてから頂点を作る
                // (途中で不良に気づいたときに、使われない頂点が残らないようにするため)
                std::vector<size_t> facePositionIndices;
                bool valid = true;
                std::string element;
                while (stream >> element)
                {
                    size_t index = 0;
                    if (!ParseFacePositionIndex(element, positions.size(), index))
                    {
                        TFE_LOG_WARN("LoadObj: {} の f の要素を読めません({}): {}", relativePath.string(), element, line);
                        valid = false;
                        break;
                    }
                    facePositionIndices.push_back(index);
                }
                if (!valid || facePositionIndices.size() < 3)
                {
                    continue; // 不良品の面は飛ばす(頂点が3つ未満では三角形にならない)
                }

                // 3-4-2: 台帳で「もう作った頂点か」を調べ、あればその番号を使い回す
                // 頂点の色は、今の材質(currentMaterial)の色。材質が違えば、同じ位置でも別の頂点になる
                std::vector<uint32_t> faceVertexIndices; // この面の頂点の番号(3-4-3 で三角形にする)
                for (const size_t positionIndex : facePositionIndices)
                {
                    const VertexKey key = { positionIndex, currentMaterial };
                    const auto [entry, isNew] = vertexLookup.try_emplace(key, static_cast<uint32_t>(mesh.vertices.size()));
                    if (isNew)
                    {
                        mesh.vertices.push_back({ positions[positionIndex], materialPalette[currentMaterial] });
                    }
                    faceVertexIndices.push_back(entry->second);
                }

                // 3-4-3: ファン分割。頂点0を軸に、扇のように三角形へ切る(N角形 → N-2 枚)
                // Z を反転して表裏が入れ替わっているので、2番目と3番目の順を逆にして直す(i, i+1 ではなく i+1, i)
                for (size_t i = 1; i + 1 < faceVertexIndices.size(); ++i)
                {
                    mesh.indices.push_back(faceVertexIndices[0]);
                    mesh.indices.push_back(faceVertexIndices[i + 1]);
                    mesh.indices.push_back(faceVertexIndices[i]);
                }
            }
            else if (keyword == "mtllib")
            {
                // .mtl は .obj と同じフォルダーにあるものとして読む
                std::string mtlName;
                if (stream >> mtlName)
                {
                    // 色見本帳の材質を、色の一覧(materialPalette)に番号付きで並べる(0 番は白なので 1 番から)
                    for (const auto& [name, color] : LoadMtl(fullPath.parent_path() / mtlName))
                    {
                        materialIndexByName[name] = materialPalette.size();
                        materialPalette.push_back(color);
                    }
                }
            }
            else if (keyword == "usemtl")
            {
                // 3-5-2: これ以降に作る頂点の材質を切り替える。色見本帳にない名前は 0 番(白)にする
                std::string materialName;
                stream >> materialName;
                const auto found = materialIndexByName.find(materialName);
                if (found != materialIndexByName.end())
                {
                    currentMaterial = found->second;
                }
                else
                {
                    TFE_LOG_WARN("LoadObj: {} に未知の材質があります: {}", relativePath.string(), materialName);
                    currentMaterial = 0;
                }
            }
            // 上記以外(コメント・空行・vn・vt・o・g など)は今は使わないので読み飛ばす
        }

        TFE_LOG_INFO("LoadObj: {} 頂点 {} 三角形 {}", relativePath.string(), mesh.vertices.size(), mesh.indices.size() / 3);

        return mesh;
    }
}
