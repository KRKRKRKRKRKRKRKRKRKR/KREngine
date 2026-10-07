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

# Dear ImGui(デバッグ用の画面)。docking 版(窓をタブでまとめたり、端にくっつけたりできる)
# ImGui は CMakeLists.txt を持たないので、取得だけして、下でライブラリを自分で定義する
FetchContent_Declare(imgui
    URL      https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b-docking.zip
    URL_HASH SHA256=0434445157a575f452ff0f2d1681fdd90ea8939e0c6983e6f8e47c51fba1bccd
)
FetchContent_MakeAvailable(imgui)

# ImGui 本体 + Win32(ウィンドウ・入力)と DX12(描画)のバックエンド
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp  # 見本の窓(ShowDemoWindow)。使い方の参考になるので入れておく
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
)
# PUBLIC = リンクした側(Engine と Game)からも imgui.h などが見える
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC d3d12 dxgi)
# /utf-8 = ImGui のソースは UTF-8 / /W0 = 他人のコードの警告は出さない(Release で assert が消えたときの未使用変数の警告など)
target_compile_options(imgui PRIVATE /utf-8 /W0)

# stb_image(PNG などの画像を、ピクセルの並びに、ほどく。ヘッダー 1 つのライブラリ)
# stb は CMakeLists.txt を持たないので、取得だけして、下で、インクルードの場所を指定する。コミットを固定(ハッシュで改ざんを検出)
FetchContent_Declare(stb
    URL      https://github.com/nothings/stb/archive/2c980bb59875b0d32144a71867fbdebb2f77cd20.zip
    URL_HASH SHA256=8e59f72b0780690cda64726804269f638a3be77b9d1506ea95f443f7964bccf0
)
FetchContent_MakeAvailable(stb)

set(ENGINE_FILES
    include/KR/KR.h
    include/KR/Core/Application.h
    include/KR/Core/Log.h
    include/KR/Core/Input.h
    include/KR/Core/Assets.h
    include/KR/Renderer/Renderer.h
    include/KR/Renderer/Camera.h
    include/KR/Renderer/Lighting.h
    include/KR/Renderer/Mesh.h
    include/KR/Renderer/Texture.h
    include/KR/Renderer/ModelLoader.h
    include/KR/Physics/Collision.h

    src/Core/Application.cpp
    src/Core/EntryPoint.cpp
    src/Core/Log.cpp
    src/Core/Input.cpp
    src/Core/Assets.cpp
    src/Core/InputState.h
    src/Platform/Windows/Window.h
    src/Platform/Windows/Window.cpp
    src/Renderer/Renderer.cpp
    src/Renderer/Camera.cpp
    src/Renderer/Mesh.cpp
    src/Renderer/ModelLoader.cpp
    src/Renderer/Image.h
    src/Renderer/Image.cpp
    src/Physics/Collision.cpp
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
    src/Graphics/DX12/DX12DepthBuffer.h
    src/Graphics/DX12/DX12DepthBuffer.cpp
    src/Graphics/DX12/DX12DescriptorHeap.h
    src/Graphics/DX12/DX12DescriptorHeap.cpp
    src/Graphics/DX12/DX12Texture.h
    src/Graphics/DX12/DX12Texture.cpp
    src/UI/ImGuiLayer.h
    src/UI/ImGuiLayer.cpp
)

# シェーダーは実行時にコンパイルする。VS が勝手に FXC でコンパイルしないよう、ビルド対象から外す
set(SHADER_FILES
    ${CMAKE_SOURCE_DIR}/Assets/Shaders/Basic.hlsl
    ${CMAKE_SOURCE_DIR}/Assets/Shaders/Common.hlsli
    ${CMAKE_SOURCE_DIR}/Assets/Shaders/Sky.hlsl
    ${CMAKE_SOURCE_DIR}/Assets/Shaders/Line.hlsl
)
set_source_files_properties(${SHADER_FILES} PROPERTIES VS_TOOL_OVERRIDE "None")

add_library(KREngine STATIC ${ENGINE_FILES} ${SHADER_FILES})

# PUBLIC = Game からも見える / PRIVATE = Engine 内部だけ
target_include_directories(KREngine
    PUBLIC  include
    PRIVATE src
)

# stb_image は、Engine の中だけで使う(PRIVATE)。SYSTEM = 他人のコードの警告は出さない
target_include_directories(KREngine SYSTEM PRIVATE ${stb_SOURCE_DIR})

target_compile_definitions(KREngine
    PUBLIC  $<$<CONFIG:Debug>:KR_DEBUG>
    PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
            # Debug ではホットリロードのため、編集中の元ファイルを直接読む
            KR_ASSETS_SOURCE_DIR="${CMAKE_SOURCE_DIR}/Assets"
)

# ImGui は PUBLIC でリンクする → Game も #include <imgui.h> で直接使える
target_link_libraries(KREngine PUBLIC imgui)

# DX12 は PRIVATE でリンクする → Game からは DX12 のヘッダーが見えない
target_link_libraries(KREngine PRIVATE
    Microsoft::DirectX-Headers
    Microsoft::DirectX-Guids
    d3d12
    dxgi
    dxcompiler
)

# Visual Studio のソリューションエクスプローラーでフォルダ構成どおりに表示する
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_FILES})
source_group(Shaders FILES ${SHADER_FILES})
set_target_properties(DirectX-Headers DirectX-Guids imgui PROPERTIES FOLDER ThirdParty)

# 実行時に必要なファイル: DXC 本体(dxcompiler.dll)とシェーダー署名用(dxil.dll)は Windows SDK のものを使う
set(PROGRAM_FILES_X86 "ProgramFiles(x86)")
set(WINDOWS_SDK_BIN_DIR "$ENV{${PROGRAM_FILES_X86}}/Windows Kits/10/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")
find_file(KR_DXCOMPILER_DLL dxcompiler.dll PATHS ${WINDOWS_SDK_BIN_DIR} NO_DEFAULT_PATH REQUIRED)
find_file(KR_DXIL_DLL dxil.dll PATHS ${WINDOWS_SDK_BIN_DIR} NO_DEFAULT_PATH REQUIRED)

# exe のビルド後に、DLL・シェーダー・モデルを exe と同じフォルダーへコピーする(Game の CMakeLists.txt から呼ぶ)
function(kr_copy_runtime_files target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${KR_DXCOMPILER_DLL} ${KR_DXIL_DLL} $<TARGET_FILE_DIR:${target}>
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_SOURCE_DIR}/Assets/Shaders $<TARGET_FILE_DIR:${target}>/Shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_SOURCE_DIR}/Assets/Models $<TARGET_FILE_DIR:${target}>/Models
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_SOURCE_DIR}/Assets/Courses $<TARGET_FILE_DIR:${target}>/Courses
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_SOURCE_DIR}/Assets/Settings $<TARGET_FILE_DIR:${target}>/Settings
        VERBATIM
    )
endfunction()
