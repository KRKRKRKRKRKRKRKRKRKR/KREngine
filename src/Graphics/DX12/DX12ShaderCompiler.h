#pragma once

#include "Graphics/DX12/DX12Common.h"

#include <dxcapi.h>

#include <filesystem>

namespace TenFiftyEight
{
    // HLSL(テキスト)を、GPU が実行できる形式(DXIL)に実行時に変換する
    class DX12ShaderCompiler
    {
    public:
        DX12ShaderCompiler();

        DX12ShaderCompiler(const DX12ShaderCompiler&) = delete;
        DX12ShaderCompiler& operator=(const DX12ShaderCompiler&) = delete;

        // entryPoint: シェーダー内の関数名(例: L"VSMain")
        // target    : シェーダーの種類とバージョン(例: L"vs_6_0" = 頂点シェーダー 6.0)
        // 失敗したときは nullptr を返し、エラー内容(ファイル名:行番号 付き)をログに出す
        ComPtr<IDxcBlob> Compile(const std::filesystem::path& file, const wchar_t* entryPoint, const wchar_t* target);

    private:
        ComPtr<IDxcUtils> m_Utils;
        ComPtr<IDxcCompiler3> m_Compiler;
        ComPtr<IDxcIncludeHandler> m_IncludeHandler; // シェーダー内の #include を解決する
    };
}
