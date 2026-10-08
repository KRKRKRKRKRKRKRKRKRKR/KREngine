#include "KR/Animation/GltfLoader.h"

#include "KR/Core/Assets.h"
#include "KR/Core/Log.h"

// cgltf の本体(関数の中身)を、このファイルに、取り込む(ほかのファイルでは、これを書かない)
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace DirectX;

namespace KR
{
    namespace
    {
        // ---- 座標系の変換(ここに集める)----
        // glTF は右手系、このエンジンは左手系。Z の符号を反転して、このエンジンの向きに直す(OBJ と同じ決まり)。
        // Z を反転した結果、三角形の表裏が入れ替わるので、メッシュでは、2番目と3番目の頂点の順を逆にして直す。

        XMFLOAT3 ConvertPosition(const float v[3])
        {
            return { v[0], v[1], -v[2] };
        }

        // クォータニオン(x, y, z, w)。Z 軸の鏡に映した回転は、x と y の符号が反転する
        XMFLOAT4 ConvertRotation(const float q[4])
        {
            return { -q[0], -q[1], q[2], q[3] };
        }

        // 4x4 行列(16 個の float)。glTF は列の順に並べてあり、そのまま読むと、DirectXMath の行ベクトルの形(転置)になる。
        // 鏡に映す:Z の成分が「片方だけ」関わる要素の符号を反転する(鏡 × 行列 × 鏡)
        XMFLOAT4X4 ConvertMatrix(const float m[16])
        {
            XMFLOAT4X4 result;
            for (int row = 0; row < 4; ++row)
            {
                for (int column = 0; column < 4; ++column)
                {
                    const float sign = ((row == 2) != (column == 2)) ? -1.0f : 1.0f;
                    result.m[row][column] = sign * m[row * 4 + column];
                }
            }
            return result;
        }

        // ---- 読み込み ----

        // ファイルを、まるごと、メモリに読む(パスに日本語が入っていても、std::filesystem なら開ける)
        bool ReadFileBytes(const std::filesystem::path& file, std::vector<uint8_t>& bytes)
        {
            std::ifstream stream(file, std::ios::binary);
            if (!stream.is_open())
            {
                return false;
            }
            bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
            return true;
        }

        const cgltf_accessor* FindAttribute(const cgltf_primitive& primitive, cgltf_attribute_type type)
        {
            for (cgltf_size i = 0; i < primitive.attributes_count; ++i)
            {
                const cgltf_attribute& attribute = primitive.attributes[i];
                if (attribute.type == type && attribute.index == 0)
                {
                    return attribute.data;
                }
            }
            return nullptr;
        }

        // メッシュ:三角形のプリミティブを、全部、1つの MeshData にまとめる
        void ReadMeshes(const cgltf_data& data, MeshData& mesh)
        {
            for (cgltf_size nodeIndex = 0; nodeIndex < data.nodes_count; ++nodeIndex)
            {
                const cgltf_mesh* gltfMesh = data.nodes[nodeIndex].mesh;
                if (!gltfMesh)
                {
                    continue;
                }

                for (cgltf_size p = 0; p < gltfMesh->primitives_count; ++p)
                {
                    const cgltf_primitive& primitive = gltfMesh->primitives[p];
                    if (primitive.type != cgltf_primitive_type_triangles)
                    {
                        KR_LOG_WARN("GltfLoader: 三角形ではないプリミティブを読み飛ばします(メッシュ {})", gltfMesh->name ? gltfMesh->name : "(名前なし)");
                        continue;
                    }

                    const cgltf_accessor* positions = FindAttribute(primitive, cgltf_attribute_type_position);
                    const cgltf_accessor* normals = FindAttribute(primitive, cgltf_attribute_type_normal);
                    const cgltf_accessor* uvs = FindAttribute(primitive, cgltf_attribute_type_texcoord);
                    if (!positions)
                    {
                        KR_LOG_WARN("GltfLoader: 位置の無いプリミティブを読み飛ばします");
                        continue;
                    }
                    if (!normals)
                    {
                        KR_LOG_WARN("GltfLoader: 法線が無いので、上向きにします");
                    }

                    // 頂点
                    const uint32_t baseVertex = static_cast<uint32_t>(mesh.vertices.size());
                    for (cgltf_size i = 0; i < positions->count; ++i)
                    {
                        float position[3] = {};
                        cgltf_accessor_read_float(positions, i, position, 3);

                        float normal[3] = { 0.0f, 1.0f, 0.0f };
                        if (normals)
                        {
                            cgltf_accessor_read_float(normals, i, normal, 3);
                        }

                        float uv[2] = {};
                        if (uvs)
                        {
                            cgltf_accessor_read_float(uvs, i, uv, 2);
                        }

                        Vertex vertex;
                        vertex.position = ConvertPosition(position);
                        vertex.normal = ConvertPosition(normal); // 向きの矢印も、Z の符号を反転する
                        vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };
                        vertex.uv = { uv[0], uv[1] };
                        mesh.vertices.push_back(vertex);
                    }

                    // 番号(3つで三角形1つ)。2番目と3番目を入れ替える(座標系の変換で、表裏が逆になるため)
                    const cgltf_size indexCount = primitive.indices ? primitive.indices->count : positions->count;
                    for (cgltf_size i = 0; i + 2 < indexCount; i += 3)
                    {
                        uint32_t triangle[3];
                        for (cgltf_size k = 0; k < 3; ++k)
                        {
                            const cgltf_size index = primitive.indices ? cgltf_accessor_read_index(primitive.indices, i + k) : (i + k);
                            triangle[k] = baseVertex + static_cast<uint32_t>(index);
                        }
                        mesh.indices.push_back(triangle[0]);
                        mesh.indices.push_back(triangle[2]);
                        mesh.indices.push_back(triangle[1]);
                    }
                }
            }
        }

        // 骨:最初のスキンを読む。骨の番号は、スキンの joints の並び(頂点が持つ番号と同じ)
        Skeleton ReadSkeleton(const cgltf_data& data, const cgltf_skin*& outSkin)
        {
            Skeleton skeleton;
            outSkin = nullptr;
            if (data.skins_count == 0)
            {
                return skeleton;
            }
            if (data.skins_count > 1)
            {
                KR_LOG_WARN("GltfLoader: スキンが {} 個あります。最初の 1 つだけ読みます", data.skins_count);
            }

            const cgltf_skin& skin = data.skins[0];
            outSkin = &skin;
            skeleton.joints.resize(skin.joints_count);

            for (cgltf_size i = 0; i < skin.joints_count; ++i)
            {
                const cgltf_node* node = skin.joints[i];
                Joint& joint = skeleton.joints[i];
                joint.name = node->name ? node->name : "(名前なし)";

                // 親:ノードの親が、スキンの joints の中の何番目か(joints の外なら、根として扱う)
                for (cgltf_size j = 0; j < skin.joints_count; ++j)
                {
                    if (skin.joints[j] == node->parent)
                    {
                        joint.parent = static_cast<int>(j);
                        break;
                    }
                }

                // 基準姿勢。行列で書いてあるときは、位置・向き・大きさに分ける
                float translation[3] = { 0.0f, 0.0f, 0.0f };
                float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                float scale[3] = { 1.0f, 1.0f, 1.0f };
                if (node->has_matrix)
                {
                    XMVECTOR s, r, t;
                    XMMATRIX m = XMMATRIX(node->matrix);
                    if (XMMatrixDecompose(&s, &r, &t, m))
                    {
                        XMFLOAT3 tf, sf;
                        XMFLOAT4 rf;
                        XMStoreFloat3(&tf, t);
                        XMStoreFloat4(&rf, r);
                        XMStoreFloat3(&sf, s);
                        translation[0] = tf.x; translation[1] = tf.y; translation[2] = tf.z;
                        rotation[0] = rf.x; rotation[1] = rf.y; rotation[2] = rf.z; rotation[3] = rf.w;
                        scale[0] = sf.x; scale[1] = sf.y; scale[2] = sf.z;
                    }
                }
                else
                {
                    if (node->has_translation) { std::copy(node->translation, node->translation + 3, translation); }
                    if (node->has_rotation)    { std::copy(node->rotation, node->rotation + 4, rotation); }
                    if (node->has_scale)       { std::copy(node->scale, node->scale + 3, scale); }
                }
                joint.translation = ConvertPosition(translation);
                joint.rotation = ConvertRotation(rotation);
                joint.scale = { scale[0], scale[1], scale[2] };

                // 逆バインド行列(無ければ単位行列)
                if (skin.inverse_bind_matrices)
                {
                    float matrix[16] = {};
                    cgltf_accessor_read_float(skin.inverse_bind_matrices, i, matrix, 16);
                    joint.inverseBind = ConvertMatrix(matrix);
                }
                else
                {
                    XMStoreFloat4x4(&joint.inverseBind, XMMatrixIdentity());
                }
            }
            return skeleton;
        }

        int FindJointIndex(const cgltf_skin& skin, const cgltf_node* node)
        {
            for (cgltf_size i = 0; i < skin.joints_count; ++i)
            {
                if (skin.joints[i] == node)
                {
                    return static_cast<int>(i);
                }
            }
            return -1;
        }

        // 動き:チャンネル(どの骨の、位置・向き・大きさか)ごとに、キー列を読む
        std::vector<AnimationClip> ReadClips(const cgltf_data& data, const cgltf_skin& skin)
        {
            std::vector<AnimationClip> clips;
            for (cgltf_size a = 0; a < data.animations_count; ++a)
            {
                const cgltf_animation& animation = data.animations[a];
                AnimationClip clip;
                clip.name = animation.name ? animation.name : "(名前なし)";

                for (cgltf_size c = 0; c < animation.channels_count; ++c)
                {
                    const cgltf_animation_channel& channel = animation.channels[c];
                    const int jointIndex = FindJointIndex(skin, channel.target_node);
                    if (jointIndex < 0)
                    {
                        continue; // 骨ではないノードの動きは、読まない
                    }
                    if (channel.target_path == cgltf_animation_path_type_weights)
                    {
                        continue; // モーフ(形の変化)は、まだ読まない
                    }

                    const cgltf_animation_sampler& sampler = *channel.sampler;
                    if (sampler.interpolation == cgltf_interpolation_type_cubic_spline)
                    {
                        KR_LOG_WARN("GltfLoader: CUBICSPLINE の補間は、まだ読めません(動き {}、骨 {})。読み飛ばします", clip.name, jointIndex);
                        continue;
                    }
                    const Interpolation interpolation = (sampler.interpolation == cgltf_interpolation_type_step) ? Interpolation::Step : Interpolation::Linear;

                    // この骨の、トラック(まだ無ければ作る)
                    auto track = std::find_if(clip.tracks.begin(), clip.tracks.end(), [&](const JointTrack& t) { return t.jointIndex == jointIndex; });
                    if (track == clip.tracks.end())
                    {
                        clip.tracks.emplace_back();
                        clip.tracks.back().jointIndex = jointIndex;
                        track = clip.tracks.end() - 1;
                    }

                    const cgltf_size keyCount = sampler.input->count;
                    float lastTime = 0.0f;
                    switch (channel.target_path)
                    {
                    case cgltf_animation_path_type_translation:
                    case cgltf_animation_path_type_scale:
                    {
                        const bool isTranslation = (channel.target_path == cgltf_animation_path_type_translation);
                        std::vector<Vec3Key>& keys = isTranslation ? track->translations : track->scales;
                        (isTranslation ? track->translationInterpolation : track->scaleInterpolation) = interpolation;
                        for (cgltf_size k = 0; k < keyCount; ++k)
                        {
                            float time = 0.0f;
                            float value[3] = {};
                            cgltf_accessor_read_float(sampler.input, k, &time, 1);
                            cgltf_accessor_read_float(sampler.output, k, value, 3);
                            Vec3Key key;
                            key.time = time;
                            key.value = isTranslation ? ConvertPosition(value) : XMFLOAT3{ value[0], value[1], value[2] };
                            keys.push_back(key);
                            lastTime = time;
                        }
                        break;
                    }
                    case cgltf_animation_path_type_rotation:
                    {
                        track->rotationInterpolation = interpolation;
                        for (cgltf_size k = 0; k < keyCount; ++k)
                        {
                            float time = 0.0f;
                            float value[4] = {};
                            cgltf_accessor_read_float(sampler.input, k, &time, 1);
                            cgltf_accessor_read_float(sampler.output, k, value, 4);
                            QuatKey key;
                            key.time = time;
                            key.value = ConvertRotation(value);
                            track->rotations.push_back(key);
                            lastTime = time;
                        }
                        break;
                    }
                    default:
                        break;
                    }
                    clip.duration = std::max(clip.duration, lastTime);
                }
                clips.push_back(std::move(clip));
            }
            return clips;
        }
    }

    GltfModel LoadGltf(const std::filesystem::path& relativePath)
    {
        const std::filesystem::path file = GetAssetDirectory("Models") / relativePath;

        // ① ファイルを、まるごと、メモリに読む
        std::vector<uint8_t> bytes;
        if (!ReadFileBytes(file, bytes))
        {
            KR_LOG_ERROR("GltfLoader: ファイルを開けません: {}", file.string());
            return {};
        }

        // ② 解析する(JSON とバイナリの部分に分けて、構造にする)
        cgltf_options options = {};
        cgltf_data* data = nullptr;
        if (cgltf_parse(&options, bytes.data(), bytes.size(), &data) != cgltf_result_success)
        {
            KR_LOG_ERROR("GltfLoader: {} を glTF として読めません", file.string());
            return {};
        }

        // ③ 形のデータ(バッファ)を読み込む。.glb は中に入っている。.gltf の別ファイル(.bin)は、file の隣から読む
        if (cgltf_load_buffers(&options, data, file.string().c_str()) != cgltf_result_success || cgltf_validate(data) != cgltf_result_success)
        {
            KR_LOG_ERROR("GltfLoader: {} のデータを読み込めない、または、壊れています", file.string());
            cgltf_free(data);
            return {};
        }

        // ④ エンジンのデータにする
        GltfModel model;
        ReadMeshes(*data, model.mesh);

        const cgltf_skin* skin = nullptr;
        model.skeleton = ReadSkeleton(*data, skin);
        if (skin)
        {
            model.clips = ReadClips(*data, *skin);
        }
        else if (data->animations_count > 0)
        {
            KR_LOG_WARN("GltfLoader: 骨(スキン)が無いので、動きは読みません");
        }

        cgltf_free(data);

        KR_LOG_INFO("GltfLoader: {} 頂点 {} 三角形 / 骨 {} 本 / 動き {} 個", model.mesh.vertices.size(), model.mesh.indices.size() / 3, model.skeleton.joints.size(), model.clips.size());
        return model;
    }
}
