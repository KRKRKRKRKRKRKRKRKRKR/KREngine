#pragma once

#include "TenFiftyEight/Renderer/Mesh.h"

#include <filesystem>

namespace TenFiftyEight
{
    // OBJ ファイルを読んで、形のデータ(CPU 側)にする。
    // relativePath は Assets/Models からの相対パス(例: "Cube/Cube.obj")
    // 失敗したらログにエラーを出して、空の MeshData を返す(CreateMesh が弾く)
    MeshData LoadObj(const std::filesystem::path& relativePath);
}
