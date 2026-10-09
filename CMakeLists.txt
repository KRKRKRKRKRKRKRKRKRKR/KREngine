cmake_minimum_required(VERSION 3.28)
# エンジンの版:メジャー.マイナー.パッチ(決まりは docs/Versioning.md。更新の記録は CHANGELOG.md)。ここが、版の、唯一の置き場所
project(KREngine VERSION 0.3.0 LANGUAGES CXX)

# このフォルダーが、最上位(エンジン単体でビルドしている)か、ゲームの一部(ゲームが、add_subdirectory している)か
# 単体のときは、C++ の規格・Visual Studio のフォルダー表示・サンプルを、ここで決める(ゲームの一部のときは、ゲーム側が決める)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(KR_IS_TOP_LEVEL ON)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
else()
    set(KR_IS_TOP_LEVEL OFF)
endif()

# サンプル(samples/)をビルドするか。単体のときは ON(エンジンの動作確認・新しいゲームの雛形)。ゲームの一部のときは OFF
option(KR_BUILD_SAMPLES "KR エンジンのサンプルをビルドする" ${KR_IS_TOP_LEVEL})

# ツール(tools/)をビルドするか。単体のときは ON。ゲームの一部のときは OFF
option(KR_BUILD_TOOLS "KR エンジンのツール(アニメーションの調整ツールなど)をビルドする" ${KR_IS_TOP_LEVEL})
option(KR_BUILD_PROBES "使い捨ての動作確認用 exe(KRBehaviorProbe など)をビルドする" OFF)

include(FetchContent)

# ゲームのアセット(モデル・コース・設定など)の場所。ゲームの CMakeLists.txt が、エンジンを取り込む前に、決める(set(KR_GAME_ASSETS_DIR ...))
# 決めてなければ、最上位の Assets/ とする(サンプルのように、ゲームのアセットが、無いときは、あってもなくても、よい)
if(NOT DEFINED KR_GAME_ASSETS_DIR)
    if(KR_IS_TOP_LEVEL)
        # エンジン単体のときは、サンプルのアセット(テスト用のモデルなど)
        set(KR_GAME_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/samples/Assets")
    else()
        set(KR_GAME_ASSETS_DIR "${CMAKE_SOURCE_DIR}/Assets")
    endif()
endif()

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

# cgltf(glTF を読む。ヘッダー 1 つのライブラリ)。エンジンだけが使う。バージョンを固定(ハッシュで改ざんを検出)
# cgltf も CMakeLists.txt の使い方が要らないので、取得だけして、下で、インクルードの場所を指定する
FetchContent_Declare(cgltf
    URL      https://github.com/jkuhlmann/cgltf/archive/refs/tags/v1.15.zip
    URL_HASH SHA256=a2f5f6db978fd269a107e9284f4a7bac5d1132cd7cc5e6464e60c8aff81001d1
)
FetchContent_MakeAvailable(cgltf)

# nlohmann/json(JSON を読み書きするライブラリ。ヘッダーだけ)。判定の形(Hitbox)を、ファイルに保存・読み込みするために使う。エンジンの中だけ(PRIVATE)
# ゲームも、同じ名前・同じ版で、取り込むことがある(Tiled のコースなど)。同じ宣言なので、競合しない(先に宣言した物が使われる)
FetchContent_Declare(nlohmann_json
    URL      https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# imnodes(ノードエディタの部品。MIT。ImGui の上に、ノードと線を描く)。敵の行動のエディタ(Behavior の GraphEditor)に使う。エンジンの中だけ(PRIVATE)
# コミット eb36902(2026 年 10 月時点の master の先頭)を固定。ハッシュで改ざんを検出
# imnodes 自身の CMakeLists.txt は使わない(imgui を探しに行くため)。SOURCE_SUBDIR に無い場所を指して、add_subdirectory を飛ばし、下で、自分で static ライブラリにする
FetchContent_Declare(imnodes
    URL      https://github.com/Nelarius/imnodes/archive/eb36902c892548ef94f88f51ad7e7c9c7058a71c.zip
    URL_HASH SHA256=45c5e408b13d5702cead6f506db50105d7d082db80199b17870e730cabea18ca
    SOURCE_SUBDIR do_not_add_subdirectory
)
FetchContent_MakeAvailable(imnodes)
add_library(imnodes STATIC ${imnodes_SOURCE_DIR}/imnodes.cpp)
target_include_directories(imnodes PUBLIC ${imnodes_SOURCE_DIR})
target_link_libraries(imnodes PUBLIC imgui)
target_compile_options(imnodes PRIVATE /utf-8 /W0)

set(ENGINE_FILES
    include/KR/KR.h
    include/KR/Core/Application.h
    include/KR/Core/Log.h
    include/KR/Core/Input.h
    include/KR/Core/Assets.h
    include/KR/Core/CommandLine.h
    include/KR/Core/Version.h
    include/KR/Renderer/Renderer.h
    include/KR/Renderer/Camera.h
    include/KR/Renderer/Lighting.h
    include/KR/Renderer/Mesh.h
    include/KR/Renderer/Texture.h
    include/KR/Renderer/ModelLoader.h
    include/KR/Physics/Collision.h
    include/KR/Animation/Skeleton.h
    include/KR/Animation/AnimationClip.h
    include/KR/Animation/GltfLoader.h
    include/KR/Animation/SkeletonDebug.h
    include/KR/Animation/AnimationPose.h
    include/KR/Animation/SkinMatrices.h
    include/KR/Animation/Hitbox.h
    include/KR/Behavior/Graph.h
    include/KR/Behavior/NodeTypes.h
    include/KR/Behavior/Context.h
    include/KR/Behavior/Runner.h
    include/KR/Behavior/GraphValidate.h
    include/KR/Behavior/GraphEditor.h
    include/KR/Behavior/GraphFile.h
    include/KR/Animation/HitboxFile.h
    include/KR/Animation/HitboxMirror.h
    include/KR/Animation/AnimatedModel.h

    src/Core/Application.cpp
    src/Core/EntryPoint.cpp
    src/Core/Log.cpp
    src/Core/Input.cpp
    src/Core/Assets.cpp
    src/Core/CommandLine.cpp
    src/Core/Version.cpp
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
    src/Animation/GltfLoader.cpp
    src/Animation/SkeletonDebug.cpp
    src/Animation/AnimationPose.cpp
    src/Animation/SkinMatrices.cpp
    src/Animation/Hitbox.cpp
    src/Animation/HitboxFile.cpp
    src/Animation/HitboxMirror.cpp
    src/Animation/AnimatedModel.cpp
    src/Behavior/Graph.cpp
    src/Behavior/NodeTypes.cpp
    src/Behavior/Context.cpp
    src/Behavior/Runner.cpp
    src/Behavior/GraphValidate.cpp
    src/Behavior/GraphEdit.h
    src/Behavior/GraphEdit.cpp
    src/Behavior/GraphEditor.cpp
    src/Behavior/Condition.h
    src/Behavior/Condition.cpp
    src/Behavior/GraphFile.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/Basic.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/Common.hlsli
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/Sky.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/Line.hlsl
)
set_source_files_properties(${SHADER_FILES} PROPERTIES VS_TOOL_OVERRIDE "None")

add_library(KREngine STATIC ${ENGINE_FILES} ${SHADER_FILES})

# PUBLIC = Game からも見える / PRIVATE = Engine 内部だけ
target_include_directories(KREngine
    PUBLIC  include
    PRIVATE src
)

# stb_image は、Engine の中だけで使う(PRIVATE)。SYSTEM = 他人のコードの警告は出さない
target_include_directories(KREngine SYSTEM PRIVATE ${stb_SOURCE_DIR} ${cgltf_SOURCE_DIR})

target_compile_definitions(KREngine
    PUBLIC  $<$<CONFIG:Debug>:KR_DEBUG>
    PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
            # エンジンの版(project の VERSION)。KR::GetVersion が返す
            KR_VERSION_MAJOR=${PROJECT_VERSION_MAJOR} KR_VERSION_MINOR=${PROJECT_VERSION_MINOR} KR_VERSION_PATCH=${PROJECT_VERSION_PATCH}
            # Debug ではホットリロードのため、編集中の元ファイルを直接読む
            KR_ASSETS_SOURCE_DIR="${KR_GAME_ASSETS_DIR}"
            # シェーダーは、エンジンの物(エンジンの shaders/)。Debug では、その元ファイルを、直接読む(F10 で再読み込みできる)
            KR_SHADER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders"
)

# ImGui は PUBLIC でリンクする → Game も #include <imgui.h> で直接使える
target_link_libraries(KREngine PUBLIC imgui)

# JSON は、エンジンの中だけで使う(PRIVATE)。公開ヘッダーには、JSON の型を出さない
target_link_libraries(KREngine PRIVATE nlohmann_json::nlohmann_json)

# imnodes も、エンジンの中だけ(PRIVATE)。ゲームの CMake は、変えなくてよい
target_link_libraries(KREngine PRIVATE imnodes)

# C++23 と UTF-8(日本語のコメント)は、エンジンのヘッダーを使う側にも必要。PUBLIC にして、ゲームに頼らない
target_compile_features(KREngine PUBLIC cxx_std_23)
target_compile_options(KREngine PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_compile_options(KREngine PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /MP>)

# DX12 は PRIVATE でリンクする → Game からは DX12 のヘッダーが見えない
target_link_libraries(KREngine PRIVATE
    Microsoft::DirectX-Headers
    Microsoft::DirectX-Guids
    d3d12
    dxgi
    dxcompiler
    shell32 # CommandLineToArgvW(GetCommandLineArguments)
)

# Visual Studio のソリューションエクスプローラーでフォルダ構成どおりに表示する
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_FILES})
source_group(Shaders FILES ${SHADER_FILES})
set_target_properties(DirectX-Headers DirectX-Guids imgui imnodes PROPERTIES FOLDER ThirdParty)

# 実行時に必要なファイル: DXC 本体(dxcompiler.dll)とシェーダー署名用(dxil.dll)は Windows SDK のものを使う
set(PROGRAM_FILES_X86 "ProgramFiles(x86)")
set(WINDOWS_SDK_BIN_DIR "$ENV{${PROGRAM_FILES_X86}}/Windows Kits/10/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")
find_file(KR_DXCOMPILER_DLL dxcompiler.dll PATHS ${WINDOWS_SDK_BIN_DIR} NO_DEFAULT_PATH REQUIRED)
find_file(KR_DXIL_DLL dxil.dll PATHS ${WINDOWS_SDK_BIN_DIR} NO_DEFAULT_PATH REQUIRED)

# exe のビルド後に、DLL・シェーダー・ゲームのアセットを、exe と同じフォルダーへコピーする(ゲームの CMakeLists.txt から呼ぶ)
#   DLL: dxcompiler.dll / dxil.dll(Windows SDK の物)
#   Shaders: エンジンの shaders/(エンジンの物)
#   ゲームのアセット: KR_GAME_ASSETS_DIR の下の、全部のフォルダー(Models・Courses・Settings など。configure のときの、一覧)。無ければ、何もしない
function(kr_copy_runtime_files target)
    set(shader_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/shaders")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${KR_DXCOMPILER_DLL} ${KR_DXIL_DLL} $<TARGET_FILE_DIR:${target}>
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${shader_dir} $<TARGET_FILE_DIR:${target}>/Shaders
        VERBATIM
    )

    if(EXISTS "${KR_GAME_ASSETS_DIR}")
        file(GLOB asset_entries LIST_DIRECTORIES true "${KR_GAME_ASSETS_DIR}/*")
        foreach(entry IN LISTS asset_entries)
            if(IS_DIRECTORY "${entry}")
                get_filename_component(folder_name "${entry}" NAME)
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${entry}" "$<TARGET_FILE_DIR:${target}>/${folder_name}"
                    VERBATIM
                )
            endif()
        endforeach()
    endif()
endfunction()

# サンプル(エンジンの使い方の見本・動作確認)。KR_BUILD_SAMPLES が ON のときだけ
if(KR_BUILD_SAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/Minimal/CMakeLists.txt")
    add_subdirectory(samples/Minimal)
    add_subdirectory(samples/Animated)
endif()

# ツール(ゲームを作るための道具。アニメーションの調整ツールなど)。KR_BUILD_TOOLS が ON のときだけ
if(KR_BUILD_TOOLS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/AnimationEditor/CMakeLists.txt")
    add_subdirectory(tools/AnimationEditor)
endif()

# 敵の行動のツール KRBehaviorEditor(ノードで、行動のグラフを作る)
if(KR_BUILD_TOOLS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/BehaviorEditor/CMakeLists.txt")
    add_subdirectory(tools/BehaviorEditor)
endif()

# Visual Studio で、F5 で実行する物(指定しないと、実行できない ALL_BUILD になる)。エンジン単体のときだけ
if(KR_IS_TOP_LEVEL)
    if(TARGET KRAnimationEditor)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT KRAnimationEditor)
    elseif(TARGET KRMinimal)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT KRMinimal)
    endif()
endif()

# 動作確認用の使い捨て exe(ライブラリの相性の確認など)。KR_BUILD_PROBES が ON のときだけ
if(KR_BUILD_PROBES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/probe/BehaviorProbe/CMakeLists.txt")
    add_subdirectory(probe/BehaviorProbe)
endif()
