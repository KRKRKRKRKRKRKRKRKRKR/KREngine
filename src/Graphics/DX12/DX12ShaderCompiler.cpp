#include "Graphics/DX12/DX12ShaderCompiler.h"

#include <string>
#include <vector>

namespace KR
{
    namespace
    {
        std::string ToUtf8(const std::filesystem::path& path)
        {
            const std::u8string utf8 = path.u8string();
            return std::string(utf8.begin(), utf8.end());
        }
    }

    DX12ShaderCompiler::DX12ShaderCompiler()
    {
        TFE_DX_CHECK(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_Utils)));
        TFE_DX_CHECK(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_Compiler)));
        TFE_DX_CHECK(m_Utils->CreateDefaultIncludeHandler(&m_IncludeHandler));
    }

    ComPtr<IDxcBlob> DX12ShaderCompiler::Compile(const std::filesystem::path& file, const wchar_t* entryPoint, const wchar_t* target)
    {
        ComPtr<IDxcBlobEncoding> source;
        if (FAILED(m_Utils->LoadFile(file.c_str(), nullptr, &source)))
        {
            TFE_LOG_ERROR("シェーダーファイルを開けません: {}", ToUtf8(file));
            return nullptr;
        }

        DxcBuffer sourceBuffer = {};
        sourceBuffer.Ptr = source->GetBufferPointer();
        sourceBuffer.Size = source->GetBufferSize();
        sourceBuffer.Encoding = DXC_CP_UTF8;

        // dxc.exe に渡すコマンドライン引数と同じもの
        const std::wstring fileName = file.wstring();
        const std::wstring includeDirectory = file.parent_path().wstring(); // #include "Common.hlsli" を探す場所(シェーダーと同じフォルダー)
        std::vector<const wchar_t*> arguments = {
            fileName.c_str(),        // エラーメッセージに表示されるファイル名
            L"-E", entryPoint,
            L"-T", target,
            L"-I", includeDirectory.c_str(),
#ifdef TFE_DEBUG
            L"-Zi", L"-Qembed_debug", // デバッグ情報を埋め込む(PIX でシェーダーをデバッグできる)
            L"-Od",                   // 最適化しない
#else
            L"-O3",                   // 最大限に最適化する
#endif
        };

        ComPtr<IDxcResult> result;
        TFE_DX_CHECK(m_Compiler->Compile(&sourceBuffer, arguments.data(), static_cast<UINT32>(arguments.size()),
                                         m_IncludeHandler.Get(), IID_PPV_ARGS(&result)));

        // エラーと警告のメッセージ(成功時も警告が入っていることがある)
        ComPtr<IDxcBlobUtf8> messages;
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&messages), nullptr);
        const bool hasMessages = messages && messages->GetStringLength() > 0;

        HRESULT status = S_OK;
        result->GetStatus(&status);
        if (FAILED(status))
        {
            TFE_LOG_ERROR("シェーダーのコンパイル失敗 ({})\n{}", ToUtf8(file.filename()),
                          hasMessages ? messages->GetStringPointer() : "");
            return nullptr;
        }
        if (hasMessages)
        {
            TFE_LOG_WARN("シェーダーの警告 ({})\n{}", ToUtf8(file.filename()), messages->GetStringPointer());
        }

        ComPtr<IDxcBlob> bytecode;
        TFE_DX_CHECK(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&bytecode), nullptr));
        return bytecode;
    }
}
