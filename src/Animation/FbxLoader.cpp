#include "KR/Animation/FbxLoader.h"

#include "KR/Core/Assets.h"
#include "KR/Core/Log.h"

// ufbx(FBX を読むライブラリ)。このファイルの中だけで使う(公開ヘッダーに、型を出さない)
#include <ufbx.h>

#include <algorithm>
#include <chrono>
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

        // 行列の軸(1〜3 行目)を、scale 倍する(位置は、変えない)。骨の世界の行列から、共通の拡大を、取り除くのに使う
        void ScaleAxes(XMFLOAT4X4& m, float scale)
        {
            for (float* axis : { &m._11, &m._21, &m._31 })
            {
                axis[0] *= scale;
                axis[1] *= scale;
                axis[2] *= scale;
            }
        }

        // 骨(Skeleton)と、骨の番号 → FBX のノード の対応(重みを読む F5・動きを読む F6 が、使う)
        struct FbxSkeleton
        {
            Skeleton skeleton;
            std::vector<const ufbx_node*> nodes; // skeleton.joints と、同じ並び
            float unitScale = 1.0f; // 骨の世界の行列に、含まれていた共通の拡大(Blender の FBX は、アーマチュアに 100 倍の拡大が付く)。骨の行列は、これで割って、拡大 1 にそろえる
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

            // 共通の拡大を、取り除く。Blender の FBX は、アーマチュアに 100 倍の拡大が付き(単位 cm との帳尻合わせ)、そのまま使うと、骨の行列の軸が長さ 100 になり、
            // 骨に付けた判定の形が、100 倍の大きさで出る(glTF の骨は、拡大 1)。最初の骨の軸の長さの平均を、共通の拡大とみなして、全部の骨(バインド・動き)の軸を、その分だけ縮める。
            // 全部の骨で、同じ拡大が掛かっていれば、スキニングの結果(世界の行列 × 逆バインド)は、変わらない(拡大が、打ち消し合う)
            if (!bindWorld.empty())
            {
                const XMMATRIX first = XMLoadFloat4x4(&bindWorld[0]);
                result.unitScale = (XMVectorGetX(XMVector3Length(first.r[0])) + XMVectorGetX(XMVector3Length(first.r[1])) + XMVectorGetX(XMVector3Length(first.r[2]))) / 3.0f;
                if (result.unitScale > 1e-6f && std::abs(result.unitScale - 1.0f) > 1e-4f)
                {
                    for (XMFLOAT4X4& world : bindWorld)
                    {
                        ScaleAxes(world, 1.0f / result.unitScale);
                    }
                }
                else
                {
                    result.unitScale = 1.0f;
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

        // 動きを焼き付ける間隔(1 秒に何回、姿勢を測るか)。FBX の動きは、曲線(補間の方法が、いろいろ)なので、一定の間隔で測って、Linear のキー列にする(Q5)。ここ 1 か所で変える
        constexpr float BakeFramesPerSecond = 30.0f;

        // 動き:アニメーションスタック(= テイク)1 つを、AnimationClip 1 つにする(名前は、スタックの名前)。レイヤーは、ufbx の評価に任せる(結果だけを焼き付ける)
        // 骨ごとに、各時刻の、世界の行列を ufbx に求めてもらい(その時刻のシーンを評価する)、親の骨から見た位置・向き・大きさに直す
        //   (基準姿勢と、同じ作り方:根は、世界の行列そのもの / 親がある骨は、親の世界の行列の逆を掛ける)。
        // 基準姿勢から、まったく動かない骨は、トラックを作らない(基準姿勢のまま)。向きは、隣のキーと反対側(符号が逆)にならないよう、そろえる(slerp が、遠回りしない)
        std::vector<AnimationClip> ReadClips(const ufbx_scene& scene, const FbxSkeleton& fbxSkeleton)
        {
            std::vector<AnimationClip> clips;
            const Skeleton& skeleton = fbxSkeleton.skeleton;
            const size_t jointCount = skeleton.joints.size();
            if (jointCount == 0)
            {
                if (scene.anim_stacks.count > 0)
                {
                    KR_LOG_WARN("FbxLoader: 骨(スキン)が無いので、動きは読みません");
                }
                return clips;
            }

            for (const ufbx_anim_stack* stack : scene.anim_stacks)
            {
                AnimationClip clip;
                clip.name = stack->name.data;
                const double length = std::max(0.0, stack->time_end - stack->time_begin);
                const size_t intervals = length > 0.0 ? static_cast<size_t>(std::ceil(length * BakeFramesPerSecond)) : 0;
                const size_t sampleCount = intervals + 1;
                clip.duration = static_cast<float>(length);

                // 各時刻の、骨の世界の行列
                std::vector<std::vector<XMFLOAT4X4>> worlds(sampleCount, std::vector<XMFLOAT4X4>(jointCount));
                std::vector<float> times(sampleCount);
                bool failed = false;
                for (size_t s = 0; s < sampleCount && !failed; ++s)
                {
                    times[s] = std::min(static_cast<float>(s) / BakeFramesPerSecond, clip.duration);
                    ufbx_error error;
                    ufbx_scene* evaluated = ufbx_evaluate_scene(&scene, stack->anim, stack->time_begin + times[s], nullptr, &error);
                    if (!evaluated)
                    {
                        KR_LOG_ERROR("FbxLoader: 動き「{}」を評価できません: {}", clip.name, error.description.data);
                        failed = true;
                        break;
                    }
                    for (size_t j = 0; j < jointCount; ++j)
                    {
                        worlds[s][j] = ConvertMatrix(evaluated->nodes.data[fbxSkeleton.nodes[j]->typed_id]->node_to_world);
                        ScaleAxes(worlds[s][j], 1.0f / fbxSkeleton.unitScale); // 骨の基準姿勢と、同じ拡大にそろえる(ReadSkeleton)
                    }
                    ufbx_free_scene(evaluated);
                }
                if (failed)
                {
                    continue;
                }

                // 骨ごとに、親から見た位置・向き・大きさにして、キー列にする
                for (size_t j = 0; j < jointCount; ++j)
                {
                    const Joint& joint = skeleton.joints[j];
                    JointTrack track;
                    track.jointIndex = static_cast<int>(j);
                    bool moves = false;
                    XMVECTOR previousRotation = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
                    for (size_t s = 0; s < sampleCount; ++s)
                    {
                        XMMATRIX local = XMLoadFloat4x4(&worlds[s][j]);
                        if (joint.parent >= 0)
                        {
                            local = local * XMMatrixInverse(nullptr, XMLoadFloat4x4(&worlds[s][static_cast<size_t>(joint.parent)]));
                        }
                        XMVECTOR scale, rotation, translation;
                        if (!XMMatrixDecompose(&scale, &rotation, &translation, local))
                        {
                            scale = XMLoadFloat3(&joint.scale);
                            rotation = XMLoadFloat4(&joint.rotation);
                            translation = XMLoadFloat3(&joint.translation);
                        }
                        if (s > 0 && XMVectorGetX(XMVector4Dot(previousRotation, rotation)) < 0.0f)
                        {
                            rotation = XMVectorNegate(rotation); // 隣のキーと、反対側の符号(同じ向き)なら、そろえる
                        }
                        previousRotation = rotation;

                        Vec3Key translationKey;
                        translationKey.time = times[s];
                        XMStoreFloat3(&translationKey.value, translation);
                        QuatKey rotationKey;
                        rotationKey.time = times[s];
                        XMStoreFloat4(&rotationKey.value, rotation);
                        Vec3Key scaleKey;
                        scaleKey.time = times[s];
                        XMStoreFloat3(&scaleKey.value, scale);
                        track.translations.push_back(translationKey);
                        track.rotations.push_back(rotationKey);
                        track.scales.push_back(scaleKey);

                        // 基準姿勢と、違うか(向きは、符号が逆でも、同じ向き)
                        auto differs = [](const XMFLOAT3& a, const XMFLOAT3& b) { return std::abs(a.x - b.x) > 1e-4f || std::abs(a.y - b.y) > 1e-4f || std::abs(a.z - b.z) > 1e-4f; };
                        const XMVECTOR baseRotation = XMLoadFloat4(&joint.rotation);
                        if (differs(translationKey.value, joint.translation) || differs(scaleKey.value, joint.scale)
                            || std::abs(XMVectorGetX(XMVector4Dot(baseRotation, rotation))) < 1.0f - 1e-6f)
                        {
                            moves = true;
                        }
                    }
                    if (moves)
                    {
                        clip.tracks.push_back(std::move(track));
                    }
                }
                clips.push_back(std::move(clip));
            }
            return clips;
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

        // スキンの重みを読んだときの、集計(ログに出す。あとから、8 本対応を判断する材料:Q9)
        struct SkinStats
        {
            size_t vertexCount = 0;      // 骨つきの頂点の数
            size_t overFourCount = 0;    // 5 本以上の骨が付いていた頂点の数(4 本に絞った)
            size_t maxInfluences = 0;    // 1 頂点に付いていた骨の、最大の本数
            size_t noWeightCount = 0;    // 重みが 1 つも無かった頂点の数(骨 0・重み 1 にした)
        };

        // 頂点 1 つの、骨の番号と重み。大きい順に 4 本まで・合計 1 に直す(5 本以上は、小さい物を捨てて、残りの合計を 1 に直す)
        //   jointOfCluster:このスキンの、クラスターの番号 → Skeleton の骨の番号
        SkinVertex ReadSkinVertex(const ufbx_skin_deformer& deformer, uint32_t vertexIndex, const std::vector<int>& jointOfCluster, SkinStats& stats)
        {
            struct Influence
            {
                int joint;
                float weight;
            };
            const ufbx_skin_vertex range = deformer.vertices.data[vertexIndex];
            std::vector<Influence> influences;
            for (uint32_t i = 0; i < range.num_weights; ++i)
            {
                const ufbx_skin_weight weight = deformer.weights.data[range.weight_begin + i];
                const int joint = weight.cluster_index < jointOfCluster.size() ? jointOfCluster[weight.cluster_index] : -1;
                if (joint >= 0 && weight.weight > 0.0)
                {
                    influences.push_back({ joint, static_cast<float>(weight.weight) });
                }
            }
            std::sort(influences.begin(), influences.end(), [](const Influence& a, const Influence& b) { return a.weight > b.weight; });

            ++stats.vertexCount;
            stats.maxInfluences = std::max(stats.maxInfluences, influences.size());
            if (influences.size() > 4)
            {
                ++stats.overFourCount;
                influences.resize(4);
            }

            SkinVertex skinVertex;
            float sum = 0.0f;
            for (const Influence& influence : influences)
            {
                sum += influence.weight;
            }
            if (sum <= 0.0f)
            {
                ++stats.noWeightCount; // 重みが無い頂点:骨 0 に、全部の重み(glTF の読み込みと同じ)
                return skinVertex;
            }
            for (size_t k = 0; k < 4; ++k)
            {
                skinVertex.joints[k] = k < influences.size() ? static_cast<uint16_t>(influences[k].joint) : 0;
                skinVertex.weights[k] = k < influences.size() ? influences[k].weight / sum : 0.0f;
            }
            return skinVertex;
        }

        // メッシュ:三角形にして、全部、1 つの MeshData にまとめる(glTF と同じ)
        // 読むメッシュ:シーンに、骨に付いたメッシュ(スキンつき)が 1 つでもあれば、**骨に付いたメッシュだけ**(骨に付いていない物 = 判定用の立方体などは、ログで知らせて、読み飛ばす)。
        //   骨に付いたメッシュが 1 つも無ければ(動かない物)、全部のメッシュを読む。(Ryusei の決定。2026-10-09)
        // 頂点は、面の角ごとに作る(法線・UV が、角ごとに違うため)。メッシュのノードの変換(置き場所・向き)を、掛ける
        // 骨つきのメッシュは、頂点ごとに、骨の番号と重み(mesh.skin)も、読む(最初のスキンだけ)。モーフ(ブレンドシェイプ)は、警告を 1 回出して、読み飛ばす
        void ReadMeshes(const ufbx_scene& scene, const FbxSkeleton& skeleton, MeshData& mesh, SkinStats& stats)
        {
            bool warnedMorph = false;
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

                if (source->blend_deformers.count > 0 && !warnedMorph)
                {
                    warnedMorph = true;
                    KR_LOG_WARN("FbxLoader: モーフ(ブレンドシェイプ)は読みません(メッシュ「{}」)", node->name.data);
                }

                // 骨つきなら:クラスターの番号 → 骨の番号(骨のノードで、探す)
                const ufbx_skin_deformer* deformer = source->skin_deformers.count > 0 ? source->skin_deformers.data[0] : nullptr;
                std::vector<int> jointOfCluster;
                if (deformer)
                {
                    if (source->skin_deformers.count > 1)
                    {
                        KR_LOG_WARN("FbxLoader: メッシュ「{}」に、スキンが {} 個あります。最初の 1 つだけ読みます", node->name.data, source->skin_deformers.count);
                    }
                    for (const ufbx_skin_cluster* cluster : deformer->clusters)
                    {
                        const auto found = std::find(skeleton.nodes.begin(), skeleton.nodes.end(), cluster->bone_node);
                        jointOfCluster.push_back(found != skeleton.nodes.end() ? static_cast<int>(found - skeleton.nodes.begin()) : -1);
                    }
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
                            if (anySkinned)
                            {
                                mesh.skin.push_back(ReadSkinVertex(*deformer, source->vertex_indices.data[corner], jointOfCluster, stats));
                            }
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

    ModelData LoadFbx(const std::filesystem::path& relativePath)
    {
        const auto startTime = std::chrono::steady_clock::now(); // 読み込みにかかった時間を、ログに出す(重い処理なので、測っておく)
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

        ModelData model;
        FbxSkeleton skeleton = ReadSkeleton(*scene);
        SkinStats skinStats;
        ReadMeshes(*scene, skeleton, model.mesh, skinStats);
        model.clips = ReadClips(*scene, skeleton); // 動きは、骨の基準姿勢(skeleton.skeleton)と比べるので、骨を、model に動かす前に読む
        model.skeleton = std::move(skeleton.skeleton);
        ufbx_free_scene(scene);

        if (model.mesh.vertices.empty())
        {
            KR_LOG_WARN("FbxLoader: {} に、読めるメッシュがありません", file.string());
            return {};
        }

        // 骨つきなら、頂点の重みの集計を出す(5 本以上の骨が付いた頂点は、4 本に絞った。あとから、8 本対応を判断する材料)
        int maxJoint = -1;
        for (const SkinVertex& skinVertex : model.mesh.skin)
        {
            for (int k = 0; k < 4; ++k)
            {
                if (skinVertex.weights[k] > 0.0f)
                {
                    maxJoint = std::max(maxJoint, static_cast<int>(skinVertex.joints[k]));
                }
            }
        }
        if (maxJoint >= static_cast<int>(model.skeleton.joints.size()))
        {
            KR_LOG_ERROR("FbxLoader: {} の頂点が、存在しない骨(番号 {}。骨は {} 本)を指しています", file.string(), maxJoint, model.skeleton.joints.size());
            return {};
        }
        if (skinStats.vertexCount > 0)
        {
            KR_LOG_INFO("FbxLoader: 骨の重み: 骨つきの頂点 {} 個・1 頂点に付く骨の最大 {} 本・5 本以上の頂点 {} 個(4 本に絞りました)・重みが無い頂点 {} 個・使う骨の番号の最大 {}",
                        skinStats.vertexCount, skinStats.maxInfluences, skinStats.overFourCount, skinStats.noWeightCount, maxJoint);
            if (skinStats.overFourCount > 0)
            {
                KR_LOG_WARN("FbxLoader: {} 個の頂点に、5 本以上の骨が付いていました(最大 {} 本)。重みの大きい 4 本に絞りました", skinStats.overFourCount, skinStats.maxInfluences);
            }
        }

        // 大きさ(最小・最大の角の差。メートル)
        XMFLOAT3 minimum = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
        XMFLOAT3 maximum = { std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
        for (const Vertex& vertex : model.mesh.vertices)
        {
            minimum = { std::min(minimum.x, vertex.position.x), std::min(minimum.y, vertex.position.y), std::min(minimum.z, vertex.position.z) };
            maximum = { std::max(maximum.x, vertex.position.x), std::max(maximum.y, vertex.position.y), std::max(maximum.z, vertex.position.z) };
        }
        KR_LOG_INFO("FbxLoader: {} 頂点 {} 三角形 / 大きさ {:.3f} × {:.3f} × {:.3f} m(範囲 x {:.2f}〜{:.2f}・y {:.2f}〜{:.2f}・z {:.2f}〜{:.2f})。骨 {} 本・動き {} 個({:.1f} fps で焼き付け)/ {} ミリ秒",
                    model.mesh.vertices.size(), model.mesh.indices.size() / 3, maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z,
                    minimum.x, maximum.x, minimum.y, maximum.y, minimum.z, maximum.z, model.skeleton.joints.size(), model.clips.size(), BakeFramesPerSecond,
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count());

        // 骨とメッシュが、離れていないか(基準姿勢で)。FBX は、Blender の置き場所を、そのまま持つので、骨とメッシュが、別々の場所にあるファイルもある。
        // その場合、骨に付けた判定の形は、メッシュから離れた所に出る(骨の動きに、メッシュが、大きく振り回されることもある)。ファイルの内容なので、読み込みでは直さず、警告で知らせる
        // 骨の位置のうち、メッシュを囲む箱に、一番近い物までの距離が、箱の対角線の半分より遠ければ、警告
        if (!model.skeleton.joints.empty())
        {
            const float diagonal = std::sqrt((maximum.x - minimum.x) * (maximum.x - minimum.x) + (maximum.y - minimum.y) * (maximum.y - minimum.y) + (maximum.z - minimum.z) * (maximum.z - minimum.z));
            float nearest = std::numeric_limits<float>::max();
            for (const Joint& joint : model.skeleton.joints)
            {
                XMFLOAT4X4 bindWorld; // 骨の、バインド時の世界の行列 = 逆バインド行列の逆。位置は 4 行目
                XMStoreFloat4x4(&bindWorld, XMMatrixInverse(nullptr, XMLoadFloat4x4(&joint.inverseBind)));
                const float dx = std::max({ minimum.x - bindWorld._41, 0.0f, bindWorld._41 - maximum.x });
                const float dy = std::max({ minimum.y - bindWorld._42, 0.0f, bindWorld._42 - maximum.y });
                const float dz = std::max({ minimum.z - bindWorld._43, 0.0f, bindWorld._43 - maximum.z });
                nearest = std::min(nearest, std::sqrt(dx * dx + dy * dy + dz * dz));
            }
            if (nearest > diagonal * 0.5f)
            {
                KR_LOG_WARN("FbxLoader: {} は、骨とメッシュが離れています(骨からメッシュを囲む箱までの距離 {:.1f} m・メッシュの大きさ(対角線){:.1f} m)。"
                            "Blender で、アーマチュアとメッシュの置き場所をそろえて(Apply Transform)、書き出し直すと、判定の形が、メッシュの上に出ます",
                            relativePath.string(), nearest, diagonal);
            }
        }
        return model;
    }
}
