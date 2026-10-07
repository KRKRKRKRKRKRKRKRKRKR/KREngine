#include "KR/Renderer/ModelLoader.h"

#include "KR/Core/Assets.h"
#include "KR/Core/Log.h"

#include <Windows.h>

#include <DirectXMath.h>

#include <charconv>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

using namespace DirectX;

namespace KR
{
    namespace
    {
        // Debug: 編集中の元フォルダー / Release: exe の横にコピーされたもの(場所の決まりは GetAssetDirectory にある)
        std::filesystem::path GetModelDirectory()
        {
            return GetAssetDirectory("Models");
        }

        // 台帳のキー: 「位置の番号」「法線の番号」「UV の番号」「材質の番号」の組。4つとも同じなら同じ頂点とみなす
        // (同じ位置でも、法線・UV・材質が違えば、別の頂点にする。頂点には、法線も UV も色も、1つしか入らないため)
        // UV の書いていない要素は、UV の番号を NoUv にする
        constexpr size_t NoUv = static_cast<size_t>(-1);
        using VertexKey = std::tuple<size_t, size_t, size_t, size_t>;

        // 面の1要素("5" "5/2" "5//1" "5/2/1" など)から取り出した、位置・UV・法線の番号(どれも棚の番号。0 始まり)
        struct FaceElement
        {
            size_t position = 0;
            bool hasUv = false;     // UV の番号が書いてあったか
            size_t uv = 0;
            bool hasNormal = false; // 法線の番号が書いてあったか(自機のように、書いていないこともある)
            size_t normal = 0;
        };

        // 材質 1 つ分の情報(.mtl の newmtl 1 つ)
        struct Material
        {
            XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // Kd(基本の色)。書いていなければ白
            std::string diffuseMap;                      // map_Kd(貼る画像のファイル名)。書いていなければ空
        };

        // 3-5-1 / 6-2: .mtl(色見本帳)を読んで、「材質の名前 → 色と画像」の表を作る。今使うのは newmtl と Kd と map_Kd だけ
        // newmtl が出たら、その材質を、白で登録する(Kd が無く、map_Kd だけの材質でも、「未知の材質」にならない)
        // 開けなければ警告を出して空の表を返す(その場合は全部白で表示される)
        std::map<std::string, Material> LoadMtl(const std::filesystem::path& mtlPath)
        {
            std::ifstream file(mtlPath);
            if (!file.is_open())
            {
                TFE_LOG_WARN("LoadObj: .mtl を開けません: {}", mtlPath.string());
                return {};
            }

            std::map<std::string, Material> materials;
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
                    materials[currentName]; // 白で登録(同じ名前が、すでにあれば、そのまま)
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
                    materials[currentName].color = { r, g, b, 1.0f };
                }
                else if (keyword == "map_Kd")
                {
                    // 画像のファイル名(.mtl と同じフォルダーにあるものとして読む。空白の入った名前は、今は読めない)
                    std::string fileName;
                    if (currentName.empty() || !(stream >> fileName))
                    {
                        TFE_LOG_WARN("LoadObj: {} の map_Kd の行を読めません: {}", mtlPath.string(), line);
                        continue;
                    }
                    materials[currentName].diffuseMap = fileName;
                }
            }
            return materials;
        }

        // OBJ の番号の文字("5" や "-1")を、棚の番号(0 始まり)にする
        // 成功したら true。数字でない・0・棚の範囲外なら false(設計図の不良)
        bool ResolveIndex(std::string_view number, size_t count, size_t& outIndex)
        {
            int value = 0;
            const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), value);
            if (error != std::errc() || end != number.data() + number.size() || value == 0)
            {
                return false;
            }

            // 正の数は 1 始まりなので 1 を引く。負の数は「棚の最後から数える」(-1 = 最後の部品)
            const long long index = value > 0 ? value - 1 : static_cast<long long>(count) + value;
            if (index < 0 || index >= static_cast<long long>(count))
            {
                return false;
            }

            outIndex = static_cast<size_t>(index);
            return true;
        }

        // 3-4-1 / 4-2 / 6-2: 面の1要素を「位置 / UV / 法線」に分けて、それぞれの番号を取り出す
        // 書き方は4通り: "5"(位置だけ) "5/2"(位置 + UV) "5//1"(位置 + 法線) "5/2/1"(全部)
        // 失敗したら false(設計図の不良)。UV・法線が書いていなければ、hasUv・hasNormal = false で成功
        bool ParseFaceElement(const std::string& element, size_t positionCount, size_t uvCount, size_t normalCount, FaceElement& out)
        {
            const std::string_view text = element;

            // 最初の「/」の手前までが位置の番号(「/」がなければ全体)
            const size_t firstSlash = text.find('/');
            if (!ResolveIndex(text.substr(0, firstSlash), positionCount, out.position))
            {
                return false;
            }

            out.hasUv = false;
            out.hasNormal = false;
            if (firstSlash == std::string_view::npos)
            {
                return true; // "5"(位置だけ)
            }

            // 1つ目と2つ目の「/」の間が UV の番号。2つ目が無ければ、最後まで("5/2")。空("5//1")なら、UV なし
            const size_t secondSlash = text.find('/', firstSlash + 1);
            const std::string_view uvText = text.substr(firstSlash + 1, secondSlash == std::string_view::npos
                                                                            ? std::string_view::npos
                                                                            : secondSlash - firstSlash - 1);
            if (!uvText.empty())
            {
                if (!ResolveIndex(uvText, uvCount, out.uv))
                {
                    return false;
                }
                out.hasUv = true;
            }

            // 2つ目の「/」の後ろが法線の番号。無い、または空("5/2/" の後ろが空)なら、法線なし
            if (secondSlash != std::string_view::npos && secondSlash + 1 < text.size())
            {
                if (!ResolveIndex(text.substr(secondSlash + 1), normalCount, out.normal))
                {
                    return false;
                }
                out.hasNormal = true;
            }
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
        std::vector<XMFLOAT3> normals;   // 向き札の棚: vn の法線 + 法線の無い面に計算で作った法線(末尾に足していく)
        std::vector<XMFLOAT2> uvs;       // UV の棚: vt の UV(画像のどこか)。v は、D3D の向き(上が 0)に直してある
        size_t computedNormalCount = 0;  // そのうち、計算で作った数(ログ用)

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
            else if (keyword == "vn")
            {
                float x, y, z;
                if (!(stream >> x >> y >> z))
                {
                    TFE_LOG_WARN("LoadObj: {} の vn の行を読めません: {}", relativePath.string(), line);
                    continue;
                }

                // 位置と同じ理由で Z を反転する。長さを 1 にそろえておく(陰影の計算は、長さ 1 の矢印を前提にしている)
                XMFLOAT3 normal;
                XMStoreFloat3(&normal, XMVector3Normalize(XMVectorSet(x, y, -z, 0.0f)));
                normals.push_back(normal);
            }
            else if (keyword == "vt")
            {
                float u, v;
                if (!(stream >> u >> v))
                {
                    TFE_LOG_WARN("LoadObj: {} の vt の行を読めません: {}", relativePath.string(), line);
                    continue;
                }

                // OBJ の v は、画像の「下」が 0。D3D(このエンジン)の v は、画像の「上」が 0。向きが逆なので、1 から引く
                uvs.push_back({ u, 1.0f - v });
            }
            else if (keyword == "f")
            {
                // 面の要素を1つずつ読んで、位置と法線の番号にする。先に全部確かめてから頂点を作る
                // (途中で不良に気づいたときに、使われない頂点が残らないようにするため)
                std::vector<FaceElement> faceElements;
                bool valid = true;
                std::string text;
                while (stream >> text)
                {
                    FaceElement element;
                    if (!ParseFaceElement(text, positions.size(), uvs.size(), normals.size(), element))
                    {
                        TFE_LOG_WARN("LoadObj: {} の f の要素を読めません({}): {}", relativePath.string(), text, line);
                        valid = false;
                        break;
                    }
                    faceElements.push_back(element);
                }
                if (!valid || faceElements.size() < 3)
                {
                    continue; // 不良品の面は飛ばす(頂点が3つ未満では三角形にならない)
                }

                // 4-2: 法線の書いていない要素があるときは、面の形から法線を計算して、向き札の棚の末尾に足す
                // (面ごとに別の番号になるので、法線の無い面は、頂点を共有しない = 面ごとのカクカクした陰影になる)
                bool needsComputedNormal = false;
                for (const FaceElement& element : faceElements)
                {
                    needsComputedNormal = needsComputedNormal || !element.hasNormal;
                }
                if (needsComputedNormal)
                {
                    // 面の最初の3頂点の、2辺の外積(2辺に垂直な矢印)。このエンジンは「時計回りが表」なので、
                    // 最終的な三角形の順(f0, f2, f1)に合わせて cross(p2 - p0, p1 - p0) にすると、外向きの法線になる
                    const XMVECTOR p0 = XMLoadFloat3(&positions[faceElements[0].position]);
                    const XMVECTOR p1 = XMLoadFloat3(&positions[faceElements[1].position]);
                    const XMVECTOR p2 = XMLoadFloat3(&positions[faceElements[2].position]);
                    const XMVECTOR cross = XMVector3Cross(XMVectorSubtract(p2, p0), XMVectorSubtract(p1, p0));

                    // 面積が 0 の三角形は、向きが決まらないので、上向きの仮の法線にする
                    XMFLOAT3 computed = { 0.0f, 1.0f, 0.0f };
                    if (XMVectorGetX(XMVector3LengthSq(cross)) > 1e-12f)
                    {
                        XMStoreFloat3(&computed, XMVector3Normalize(cross));
                    }
                    normals.push_back(computed);
                    ++computedNormalCount;

                    for (FaceElement& element : faceElements)
                    {
                        if (!element.hasNormal)
                        {
                            element.hasNormal = true;
                            element.normal = normals.size() - 1;
                        }
                    }
                }

                // 3-4-2: 台帳で「もう作った頂点か」を調べ、あればその番号を使い回す
                // 頂点の色は、今の材質(currentMaterial)の色。位置・法線・UV・材質のどれかが違えば、別の頂点になる
                std::vector<uint32_t> faceVertexIndices; // この面の頂点の番号(3-4-3 で三角形にする)
                for (const FaceElement& element : faceElements)
                {
                    const VertexKey key = { element.position, element.normal, element.hasUv ? element.uv : NoUv, currentMaterial };
                    const auto [entry, isNew] = vertexLookup.try_emplace(key, static_cast<uint32_t>(mesh.vertices.size()));
                    if (isNew)
                    {
                        Vertex vertex;
                        vertex.position = positions[element.position];
                        vertex.normal = normals[element.normal];
                        vertex.color = materialPalette[currentMaterial];
                        vertex.uv = element.hasUv ? uvs[element.uv] : XMFLOAT2{ 0.0f, 0.0f };
                        mesh.vertices.push_back(vertex);
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
                    for (const auto& [name, material] : LoadMtl(fullPath.parent_path() / mtlName))
                    {
                        materialIndexByName[name] = materialPalette.size();
                        materialPalette.push_back(material.color);

                        // 6-2 貼る画像。今は、形ごとに 1 枚だけ(材質ごとに別の画像は、まだ扱えない)。最初に見つかったものを使う
                        if (!material.diffuseMap.empty())
                        {
                            const std::filesystem::path texturePath = fullPath.parent_path() / material.diffuseMap;
                            if (mesh.diffuseTexturePath.empty())
                            {
                                mesh.diffuseTexturePath = texturePath;
                            }
                            else if (mesh.diffuseTexturePath != texturePath)
                            {
                                TFE_LOG_WARN("LoadObj: {} の材質 {} の画像({})は、使えません(今は、形ごとに 1 枚だけ)",
                                    relativePath.string(), name, material.diffuseMap);
                            }
                        }
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
            // 上記以外(コメント・空行・vt・o・g など)は今は使わないので読み飛ばす
        }

        TFE_LOG_INFO("LoadObj: {} 頂点 {} 三角形 {} 法線(ファイル {} / 計算 {}) UV {}", relativePath.string(),
            mesh.vertices.size(), mesh.indices.size() / 3, normals.size() - computedNormalCount, computedNormalCount, uvs.size());
        if (!mesh.diffuseTexturePath.empty())
        {
            TFE_LOG_INFO("LoadObj: {} の画像: {}", relativePath.string(), mesh.diffuseTexturePath.filename().string());
        }

        return mesh;
    }
}
