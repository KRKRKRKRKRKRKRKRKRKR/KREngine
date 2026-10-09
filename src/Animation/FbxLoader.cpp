#include "KR/Animation/FbxLoader.h"

#include "KR/Core/Assets.h"
#include "KR/Core/Log.h"

// ufbx(FBX を読むライブラリ)。このファイルの中だけで使う(公開ヘッダーに、型を出さない)
#include <ufbx.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <utility>
#include <string>
#include <vector>

using namespace DirectX;

namespace KR
{
    namespace
    {
        // ---- 座標系の変換(ここに集める)----
        // ufbx には、**右手系・Y が上・メートル**(glTF と同じ向き)で出してもらい(読み込みの設定)、そのあと、GltfLoader.cpp と、**同じ決まり**で、このエンジン(左手系)に直す:
        // Z の符号を反転する・三角形の 2 番目と 3 番目の順を、入れ替える(表裏が、入れ替わるため)。
        // ufbx に、左手系(ufbx_axes_left_handed_y_up)で出してもらう案は、選ばなかった:この FBX(Blender から書き出した物)で試すと、
        // 上下が逆さま(Y が反転)になり、glTF の読み込み結果と、合わなかった。右手系で出すと、glTF と、向き・大きさが、一致した(2026-10-09 に確認)
        // 単位は、ufbx が、メートルに直す(FBX は、cm のことが多い)

        XMFLOAT3 ConvertPosition(const ufbx_vec3& v)
        {
            return { static_cast<float>(v.x), static_cast<float>(v.y), -static_cast<float>(v.z) };
        }

        // ufbx の行列(列ごとの 3 × 4:軸 3 本 + 位置。右手系)→ DirectXMath の行列(行ベクトルの形・左手系)
        // 右手系 → 左手系は、Z を鏡にする:Z の添え字が「ひとつだけ」関わる要素(_13 _23 _31 _32 _43)の符号を反転する(GltfLoader の ConvertMatrix と同じ)
        XMFLOAT4X4 ConvertMatrix(const ufbx_matrix& m)
        {
            XMFLOAT4X4 result;
            result._11 = static_cast<float>(m.cols[0].x); result._12 = static_cast<float>(m.cols[0].y); result._13 = -static_cast<float>(m.cols[0].z); result._14 = 0.0f;
            result._21 = static_cast<float>(m.cols[1].x); result._22 = static_cast<float>(m.cols[1].y); result._23 = -static_cast<float>(m.cols[1].z); result._24 = 0.0f;
            result._31 = -static_cast<float>(m.cols[2].x); result._32 = -static_cast<float>(m.cols[2].y); result._33 = static_cast<float>(m.cols[2].z); result._34 = 0.0f;
            result._41 = static_cast<float>(m.cols[3].x); result._42 = static_cast<float>(m.cols[3].y); result._43 = -static_cast<float>(m.cols[3].z); result._44 = 1.0f;
            return result;
        }

        // 骨(Skeleton)と、骨の番号 → FBX のノード の対応(重みを読む F5・動きを読む F6 が、使う)
        struct FbxSkeleton
        {
            Skeleton skeleton;
            std::vector<const ufbx_node*> nodes; // skeleton.joints と、同じ並び
        };

        // 骨:スキン(ufbx_skin_deformer)の「クラスター」(= 骨 1 本の、影響の設定)が指すノードを、骨にする。番号は、クラスターの並び
        //   (複数のスキンがあれば、順に足す。同じ骨は、1 回だけ)。重みの付いていない骨(先端の _end など)は、骨にならない(glTF の、スキンの joints と同じ考え方)
        // 基準姿勢 = クラスターの bind_to_world(骨の、バインド時の世界の行列)。アニメーションの最初の時刻の姿勢ではなく、**バインド姿勢**
        //   逆バインド行列 = bind_to_world の逆。親 = 祖先のうち、骨になっている、最も近い物(間のノードは、飛ばす。位置・向きは、世界の行列の差で、正しく出る)
        //   根(親の骨が無い)は、基準姿勢 = 世界の行列そのもの・parentWorld は単位行列(モデルの座標 = FBX の世界の座標)
        FbxSkeleton ReadSkeleton(const ufbx_scene& scene)
        {
            FbxSkeleton result;
            std::vector<XMFLOAT4X4> bindWorld;
            for (const ufbx_skin_deformer* deformer : scene.skin_deformers)
            {
                for (const ufbx_skin_cluster* cluster : deformer->clusters)
                {
                    if (!cluster->bone_node || std::find(result.nodes.begin(), result.nodes.end(), cluster->bone_node) != result.nodes.end())
                    {
                        continue;
                    }
                    result.nodes.push_back(cluster->bone_node);
                    bindWorld.push_back(ConvertMatrix(cluster->bind_to_world));
                }
            }

            result.skeleton.joints.resize(result.nodes.size());
            for (size_t i = 0; i < result.nodes.size(); ++i)
            {
                Joint& joint = result.skeleton.joints[i];
                joint.name = result.nodes[i]->name.data;

                for (const ufbx_node* ancestor = result.nodes[i]->parent; ancestor && joint.parent < 0; ancestor = ancestor->parent)
                {
                    const auto found = std::find(result.nodes.begin(), result.nodes.end(), ancestor);
                    if (found != result.nodes.end())
                    {
                        joint.parent = static_cast<int>(found - result.nodes.begin());
                    }
                }

                const XMMATRIX world = XMLoadFloat4x4(&bindWorld[i]);
                XMMATRIX local = world; // 根:世界の行列そのもの
                if (joint.parent >= 0)
                {
                    local = world * XMMatrixInverse(nullptr, XMLoadFloat4x4(&bindWorld[static_cast<size_t>(joint.parent)]));
                }
                XMVECTOR scale, rotation, translation;
                if (XMMatrixDecompose(&scale, &rotation, &translation, local))
                {
                    XMStoreFloat3(&joint.scale, scale);
                    XMStoreFloat4(&joint.rotation, rotation);
                    XMStoreFloat3(&joint.translation, translation);
                }
                XMStoreFloat4x4(&joint.inverseBind, XMMatrixInverse(nullptr, world));
            }
            return result;
        }

        // ファイルを、まるごと、メモリに読む。path は、std::filesystem::path のまま開く
        // (日本語のフォルダーでも、動く。ufbx に、パスの文字列を渡さず、メモリの中身を渡すのも、そのため)
        bool ReadFileBytes(const std::filesystem::path& path, std::vector<uint8_t>& bytes)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                return false;
            }
            bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
            return true;
        }

        // メッシュ:三角形にして、全部、1 つの MeshData にまとめる(glTF と同じ)
        // 読むメッシュ:シーンに、骨に付いたメッシュ(スキンつき)が 1 つでもあれば、**骨に付いたメッシュだけ**(骨に付いていない物 = 判定用の立方体などは、ログで知らせて、読み飛ばす)。
        //   骨に付いたメッシュが 1 つも無ければ(動かない物)、全部のメッシュを読む。(Ryusei の決定。2026-10-09)
        // 頂点は、面の角ごとに作る(法線・UV が、角ごとに違うため)。メッシュのノードの変換(置き場所・向き)を、掛ける
        void ReadMeshes(const ufbx_scene& scene, MeshData& mesh)
        {
            bool anySkinned = false;
            for (const ufbx_node* node : scene.nodes)
            {
                if (node->mesh && node->mesh->skin_deformers.count > 0)
                {
                    anySkinned = true;
                }
            }

            std::vector<uint32_t> triangleCorners; // 1 つの面を、三角形にしたときの、角の番号
            for (const ufbx_node* node : scene.nodes)
            {
                const ufbx_mesh* source = node->mesh;
                if (!source)
                {
                    continue;
                }
                if (anySkinned && source->skin_deformers.count == 0)
                {
                    KR_LOG_INFO("FbxLoader: 骨に付いていないメッシュ「{}」(三角形 {})を、読み飛ばします", node->name.data, source->num_triangles);
                    continue;
                }

                triangleCorners.resize(source->max_face_triangles * 3);
                for (const ufbx_face& face : source->faces)
                {
                    const uint32_t triangleCount = ufbx_triangulate_face(triangleCorners.data(), triangleCorners.size(), source, face);
                    for (uint32_t t = 0; t < triangleCount; ++t)
                    {
                        const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
                        for (uint32_t k = 0; k < 3; ++k)
                        {
                            const uint32_t corner = triangleCorners[t * 3 + k];

                            Vertex vertex;
                            vertex.position = ConvertPosition(ufbx_transform_position(&node->geometry_to_world, ufbx_get_vertex_vec3(&source->vertex_position, corner)));
                            vertex.normal = { 0.0f, 1.0f, 0.0f };
                            if (source->vertex_normal.exists)
                            {
                                const ufbx_vec3 normal = ufbx_transform_direction(&node->geometry_to_world, ufbx_get_vertex_vec3(&source->vertex_normal, corner));
                                XMFLOAT3 converted = ConvertPosition(normal); // 向きの矢印も、Z の符号を反転する
                                const float length = std::sqrt(converted.x * converted.x + converted.y * converted.y + converted.z * converted.z);
                                if (length > 1e-12f)
                                {
                                    vertex.normal = { converted.x / length, converted.y / length, converted.z / length };
                                }
                            }
                            vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };
                            if (source->vertex_uv.exists)
                            {
                                const ufbx_vec2 uv = ufbx_get_vertex_vec2(&source->vertex_uv, corner);
                                vertex.uv = { static_cast<float>(uv.x), 1.0f - static_cast<float>(uv.y) }; // FBX の v は、下が 0。このエンジンは、左上が (0, 0)
                            }
                            mesh.vertices.push_back(vertex);
                        }
                        // 番号(3つで三角形1つ)。2番目と3番目を入れ替える(座標系の変換で、表裏が逆になるため)
                        mesh.indices.push_back(base);
                        mesh.indices.push_back(base + 2);
                        mesh.indices.push_back(base + 1);
                    }
                }
            }
        }
    }

    GltfModel LoadFbx(const std::filesystem::path& relativePath)
    {
        const std::filesystem::path file = GetAssetDirectory("Models") / relativePath;

        std::vector<uint8_t> bytes;
        if (!ReadFileBytes(file, bytes))
        {
            KR_LOG_ERROR("FbxLoader: ファイルを開けません: {}", file.string());
            return {};
        }

        // 右手系・Y が上・メートルで出してもらう(上の「座標系の変換」を読む)。ufbx が、軸と単位の違いを、読み込み時に直す
        ufbx_load_opts options = {};
        options.target_axes = ufbx_axes_right_handed_y_up;
        options.target_unit_meters = 1.0f;
        options.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
        options.generate_missing_normals = true;

        ufbx_error error;
        ufbx_scene* scene = ufbx_load_memory(bytes.data(), bytes.size(), &options, &error);
        if (!scene)
        {
            KR_LOG_ERROR("FbxLoader: {} を FBX として読めません: {}", file.string(), error.description.data);
            return {};
        }

        GltfModel model;
        ReadMeshes(*scene, model.mesh);
        FbxSkeleton skeleton = ReadSkeleton(*scene);
        model.skeleton = std::move(skeleton.skeleton);
        ufbx_free_scene(scene);

        if (model.mesh.vertices.empty())
        {
            KR_LOG_WARN("FbxLoader: {} に、読めるメッシュがありません", file.string());
            return {};
        }

        // 大きさ(最小・最大の角の差。メートル)
        XMFLOAT3 minimum = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
        XMFLOAT3 maximum = { std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
        for (const Vertex& vertex : model.mesh.vertices)
        {
            minimum = { std::min(minimum.x, vertex.position.x), std::min(minimum.y, vertex.position.y), std::min(minimum.z, vertex.position.z) };
            maximum = { std::max(maximum.x, vertex.position.x), std::max(maximum.y, vertex.position.y), std::max(maximum.z, vertex.position.z) };
        }
        KR_LOG_INFO("FbxLoader: {} 頂点 {} 三角形 / 大きさ {:.3f} × {:.3f} × {:.3f} m(範囲 x {:.2f}〜{:.2f}・y {:.2f}〜{:.2f}・z {:.2f}〜{:.2f})。骨 {} 本(重み・動きは、まだ読みません)",
                    model.mesh.vertices.size(), model.mesh.indices.size() / 3, maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z,
                    minimum.x, maximum.x, minimum.y, maximum.y, minimum.z, maximum.z, model.skeleton.joints.size());
        return model;
    }
}
