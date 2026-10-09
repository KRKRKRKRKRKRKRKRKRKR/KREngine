#include "KR/Animation/FbxLoader.h"

#include "KR/Core/Assets.h"
#include "KR/Core/Log.h"

// ufbx(FBX を読むライブラリ)。このファイルの中だけで使う(公開ヘッダーに、型を出さない)
#include <ufbx.h>

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace KR
{
    namespace
    {
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

        // エンジンの座標(左手系・Y が上・メートル)にそろえる設定(読み込み時に、ufbx が変換する)
        ufbx_load_opts options = {};
        options.target_axes = ufbx_axes_left_handed_y_up;
        options.target_unit_meters = 1.0f;
        options.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;

        ufbx_error error;
        ufbx_scene* scene = ufbx_load_memory(bytes.data(), bytes.size(), &options, &error);
        if (!scene)
        {
            KR_LOG_ERROR("FbxLoader: {} を FBX として読めません: {}", file.string(), error.description.data);
            return {};
        }

        KR_LOG_INFO("FbxLoader: {} を解析しました(ufbx: ノード {} 個・メッシュ {} 個・スキン {} 個・動き {} 個)。中身は、まだ読みません",
                    relativePath.string(), scene->nodes.count, scene->meshes.count, scene->skin_deformers.count, scene->anim_stacks.count);
        ufbx_free_scene(scene);
        return {};
    }
}
