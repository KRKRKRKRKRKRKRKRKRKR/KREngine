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
    src/Graphics/DX12/DX12ShaderCompiler.h
    src/Graphics/DX12/DX12ShaderCompiler.cpp
    src/Graphics/DX12/DX12Pipeline.h
    src/Graphics/DX12/DX12Pipeline.cpp
    src/Graphics/DX12/DX12Buffer.h
    src/Graphics/DX12/DX12Buffer.cpp
)

# シェーダーは実行時にコンパイルする。VS が勝手に FXC でコンパイルしないよう、ビルド対象から外す
set(SHADER_FILES
    ${CMAKE_SOURCE_DIR}/Assets/Shaders/Basic.hlsl
)
set_source_files_properties(${SHADER_FILES} PROPERTIES VS_TOOL_OVERRIDE "None")

add_library(TenFiftyEight STATIC ${ENGINE_FILES} ${SHADER_FILES})

# PUBLIC = Game からも見える / PRIVATE = Engine 内部だけ
target_include_directories(TenFiftyEight
    PUBLIC  include
    PRIVATE src
)

target_compile_definitions(TenFiftyEight
    PUBLIC  $<$<CONFIG:Debug>:TFE_DEBUG>
    PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
            # Debug ではホットリロードのため、編集中の元ファイルを直接読む
            TFE_SHADER_SOURCE_DIR="${CMAKE_SOURCE_DIR}/Assets/Shaders"
)

# DX12 は PRIVATE でリンクする → Game からは DX12 のヘッダーが見えない
target_link_libraries(TenFiftyEight PRIVATE
    Microsoft::DirectX-Headers
    Microsoft::DirectX-Guids
    d3d12
    dxgi
    dxcompiler
)

# Visual Studio のソリューションエクスプローラーでフォルダ構成どおりに表示する
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_FILES})
source_group(Shaders FILES ${SHADER_FILES})
set_target_properties(DirectX-Headers DirectX-Guids PROPERTIES FOLDER ThirdParty)

# 実行時に必要なファイル: DXC 本体(dxcompiler.dll)とシェーダー署名用(dxil.dll)は Windows SDK のものを使う
set(PROGRAM_FILES_X86 "ProgramFiles(x86)")
set(WINDOWS_SDK_BIN_DIR "$ENV{${PROGRAM_FILES_X86}}/Windows Kits/10/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")
find_file(TFE_DXCOMPILER_DLL dxcompiler.dll PATHS ${WINDOWS_SDK_BIN_DIR} NO_DEFAULT_PATH REQUIRED)
find_file(TFE_DXIL_DLL dxil.dll PATHS ${WINDOWS_SDK_BIN_DIR} NO_DEFAULT_PATH REQUIRED)

# exe のビルド後に、DLL とシェーダーを exe と同じフォルダーへコピーする(Game の CMakeLists.txt から呼ぶ)
function(tfe_copy_runtime_files target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TFE_DXCOMPILER_DLL} ${TFE_DXIL_DLL} $<TARGET_FILE_DIR:${target}>
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_SOURCE_DIR}/Assets/Shaders $<TARGET_FILE_DIR:${target}>/Shaders
        VERBATIM
    )
endfunction()
