#include "KR/Animation/LoadModel.h"

#include "KR/Animation/FbxLoader.h"
#include "KR/Core/Log.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace KR
{
    ModelData LoadModel(const std::filesystem::path& relativePath)
    {
        // 拡張子(ASCII だけ、小文字にして比べる)
        std::string extension = relativePath.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (extension == ".glb" || extension == ".gltf")
        {
            return LoadGltf(relativePath);
        }
        if (extension == ".fbx")
        {
            return LoadFbx(relativePath);
        }
        KR_LOG_ERROR("LoadModel: 読めない形式です(拡張子「{}」。.glb / .gltf / .fbx に対応): {}", extension, relativePath.string());
        return {};
    }
}
