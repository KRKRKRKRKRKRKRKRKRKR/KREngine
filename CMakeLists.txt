include(FetchContent)

# d3dx12.h などの DX12 ヘッダー(Git 不要の zip ダウンロード。ハッシュで改ざんを検出する)
FetchContent_Declare(DirectX-Headers
    URL      https://github.com/microsoft/DirectX-Headers/archive/refs/tags/v1.618.2.zip
    URL_HASH SHA256=89D146E806FDC2CDC0FE8AA54731AE039C6FF5A6D88FE002DF726FE80AE565FD
)
set(DXHEADERS_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(DXHEADERS_BUILD_GOOGLE_TEST OFF CACHE BOOL "" FORCE)
set(DXHEADERS_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(DirectX-Headers)

set(ENGINE_FILES
    include/TenFiftyEight/TenFiftyEight.h
    include/TenFiftyEight/Core/Application.h
    include/TenFiftyEight/Core/Log.h
    include/TenFiftyEight/Renderer/Renderer.h

    src/Core/Application.cpp
    src/Core/EntryPoint.cpp
    src/Core/Log.cpp
    src/Platform/Windows/Window.h
    src/Platform/Windows/Window.cpp
    src/Renderer/Renderer.cpp
    src/Graphics/DX12/DX12Common.h
    src/Graphics/DX12/DX12Device.h
    src/Graphics/DX12/DX12Device.cpp
    src/Graphics/DX12/DX12CommandQueue.h
    src/Graphics/DX12/DX12CommandQueue.cpp
    src/Graphics/DX12/DX12SwapChain.h
    src/Graphics/DX12/DX12SwapChain.cpp
    src/Graphics/DX12/DX12CommandList.h
    src/Graphics/DX12/DX12CommandList.cpp
)

add_library(TenFiftyEight STATIC ${ENGINE_FILES})

# PUBLIC = Game からも見える / PRIVATE = Engine 内部だけ
target_include_directories(TenFiftyEight
    PUBLIC  include
    PRIVATE src
)

target_compile_definitions(TenFiftyEight
    PUBLIC  $<$<CONFIG:Debug>:TFE_DEBUG>
    PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
)

# DX12 は PRIVATE でリンクする → Game からは DX12 のヘッダーが見えない
target_link_libraries(TenFiftyEight PRIVATE
    Microsoft::DirectX-Headers
    Microsoft::DirectX-Guids
    d3d12
    dxgi
)

# Visual Studio のソリューションエクスプローラーでフォルダ構成どおりに表示する
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_FILES})
set_target_properties(DirectX-Headers DirectX-Guids PROPERTIES FOLDER ThirdParty)
