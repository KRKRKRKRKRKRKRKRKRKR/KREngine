#pragma once

// directx/ 付きのヘッダーは DirectX-Headers(Windows SDK より新しい版)
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "TenFiftyEight/Core/Log.h"

#include <cstdint>

namespace TenFiftyEight
{
    // COM オブジェクト用のスマートポインタ。スコープを抜けると自動で Release() される
    using Microsoft::WRL::ComPtr;

    constexpr uint32_t BackBufferCount = 2;
}

// DX12 / DXGI の関数は HRESULT で成否を返す。失敗したら式と HRESULT を出して停止する
#define TFE_DX_CHECK(expression)                                                        \
    do                                                                                  \
    {                                                                                   \
        const HRESULT tfeHr = (expression);                                             \
        if (FAILED(tfeHr))                                                              \
        {                                                                               \
            TFE_LOG_FATAL("{} 失敗 (HRESULT 0x{:08X})", #expression,                    \
                          static_cast<uint32_t>(tfeHr));                                \
        }                                                                               \
    } while (0)
