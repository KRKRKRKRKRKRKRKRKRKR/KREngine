#pragma once

#include <filesystem>
#include <vector>

namespace KR
{
    // 起動したときの、コマンドラインの引数(先頭 = exe のパス)。日本語などを含むパスも、正しく読める
    // main() の形は、変えずに、Windows の関数で取り出す(OnInit などで、いつでも呼べる)
    std::vector<std::filesystem::path> GetCommandLineArguments();
}
