#pragma once

// directx/ 付きのヘッダーは DirectX-Headers(Windows SDK より新しい版)
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "KR/Core/Log.h"

#include <cstdint>

namespace KR
{
    // COM オブジェクト用のスマートポインタ。スコープを抜けると自動で Release() される
    using Microsoft::WRL::ComPtr;

    constexpr uint32_t BackBufferCount = 2;

    // 描画先の形式。_SRGB にすると、シェーダーが出力した「物理的に正しい明るさ」の色を、
    // 書き込むときにモニター用の明るさ(ガンマ補正)へ自動で変換してくれる
    constexpr DXGI_FORMAT RenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    // 同じバックバッファを、変換なしで書き込む見方(UNORM)。ImGui のように、色が最初からモニター用の値になっているものに使う
    constexpr DXGI_FORMAT LinearRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
}

// DX12 / DXGI の関数は HRESULT で成否を返す。失敗したら式と HRESULT を出して停止する
#define KR_DX_CHECK(expression)                                                        \
    do                                                                                  \
    {                                                                                   \
        const HRESULT tfeHr = (expression);                                             \
        if (FAILED(tfeHr))                                                              \
        {                                                                               \
            KR_LOG_FATAL("{} 失敗 (HRESULT 0x{:08X})", #expression,                    \
                          static_cast<uint32_t>(tfeHr));                                \
        }                                                                               \
    } while (0)
