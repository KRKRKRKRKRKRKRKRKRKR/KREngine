cmake_minimum_required(VERSION 3.28)
# エンジンの版:メジャー.マイナー.パッチ(決まりは docs/Versioning.md。更新の記録は CHANGELOG.md)。ここが、版の、唯一の置き場所
project(KREngine VERSION 0.16.0 LANGUAGES CXX)

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
        # 決めた既定値を、ゲーム側の CMake にも見せる(kr_add_game・kr_copy_runtime_files が、ゲーム側から呼ばれて、この値を読むため。
        # add_subdirectory の中で決めた変数は、PARENT_SCOPE を付けないと、呼び出し元には戻らない)
        set(KR_GAME_ASSETS_DIR "${KR_GAME_ASSETS_DIR}" PARENT_SCOPE)
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
# 同じ stb の中の stb_vorbis.c(OGG の音を、PCM に、ほどく)も、音の読み込み(src/Audio/)で使う

# dr_wav・dr_mp3(WAV・MP3 の音を、PCM に、ほどく。どちらもヘッダー 1 つのライブラリ。パブリックドメイン / MIT-0)。エンジンの中だけ(PRIVATE)
# dr_libs の、コミット dfe8377(2026-09-01。dr_wav 0.14.6・dr_mp3 0.7.4)を固定。ハッシュで改ざんを検出。CMakeLists.txt は使わない(取得だけ)
FetchContent_Declare(dr_libs
    URL      https://github.com/mackron/dr_libs/archive/dfe8377631000664666519fdb83da193fd8037f4.zip
    URL_HASH SHA256=1d7d0f3ffa117601ab848d7b191e68332b9b3610a049c81fc3e109dffa78bf69
    SOURCE_SUBDIR do_not_add_subdirectory
)
FetchContent_MakeAvailable(dr_libs)

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

# ufbx(FBX を読む)。エンジンの中だけで使う(PRIVATE)。**版を固定**(v0.23.1・指すコミット 26a482ae66871d7de36eb722aa060bce95bce274)
# ufbx は、0.Y.Z の間は、マイナーが上がるたびに互換が壊れることがある。上げるときは、CHANGELOG を読んでから
# 取得だけして(SOURCE_SUBDIR を、存在しない場所にして、add_subdirectory を避ける)、ライブラリは、third_party/ufbx で定義する
FetchContent_Declare(ufbx
    URL      https://github.com/ufbx/ufbx/archive/refs/tags/v0.23.1.zip
    URL_HASH SHA256=3e5d8219f5b634fd374949808206eba1a1da49b2b2e3ee31ff0bd228c0abd469
    SOURCE_SUBDIR do_not_add_subdirectory
)
FetchContent_MakeAvailable(ufbx)
set(UFBX_SOURCE_DIR ${ufbx_SOURCE_DIR})
add_subdirectory(third_party/ufbx)

# imgui-node-editor(ノードエディタの部品。MIT。ズーム・スクロールがあり、見た目が UE のブループリント風)。敵の行動のエディタ(Behavior の GraphEditor)に使う。エンジンの中だけ(PRIVATE)
# imnodes に、ズームの機能が無いため、替える(docs/Behavior.md)。コミット 1e0e494(2026-10-14 の master の先頭)を固定。ハッシュで改ざんを検出
# 自身の CMakeLists.txt は無い。取得だけして(SOURCE_SUBDIR を、存在しない場所にして)、ライブラリは、下で、自分で定義する
FetchContent_Declare(imgui_node_editor
    URL      https://github.com/thedmd/imgui-node-editor/archive/1e0e494f1a86885eb332ca518543108ccd88d13e.zip
    URL_HASH SHA256=15095ca5610bd93497c2ee726df9f23c5a7121f9d4456f3650176cb9dedeb342
    SOURCE_SUBDIR do_not_add_subdirectory
)
FetchContent_MakeAvailable(imgui_node_editor)
add_library(imgui_node_editor STATIC
    ${imgui_node_editor_SOURCE_DIR}/crude_json.cpp
    ${imgui_node_editor_SOURCE_DIR}/imgui_canvas.cpp
    ${imgui_node_editor_SOURCE_DIR}/imgui_node_editor.cpp
    ${imgui_node_editor_SOURCE_DIR}/imgui_node_editor_api.cpp
)
target_include_directories(imgui_node_editor PUBLIC ${imgui_node_editor_SOURCE_DIR})
target_link_libraries(imgui_node_editor PUBLIC imgui)
target_compile_options(imgui_node_editor PRIVATE /utf-8 /W0)

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
    include/KR/Behavior/GraphDebug.h
    include/KR/Behavior/GraphAsset.h
    include/KR/Behavior/LiveGraph.h
    include/KR/Behavior/Observe.h
    include/KR/Behavior/GraphFile.h
    include/KR/Animation/HitboxFile.h
    include/KR/Animation/HitboxMirror.h
    include/KR/Animation/FbxLoader.h
    include/KR/Animation/LoadModel.h
    include/KR/Animation/AnimatedModel.h
    include/KR/Audio/Audio.h

    src/Core/Application.cpp
    src/Core/EntryPoint.cpp
    src/Core/Log.cpp
    src/Core/Input.cpp
    src/Core/Assets.cpp
    src/Core/CommandLine.cpp
    src/Core/Version.cpp
    src/Core/InputState.h
    src/Core/EditorLauncher.h
    src/Core/EditorLauncher.cpp
    src/Platform/Windows/Window.h
    src/Platform/Windows/KeyCodes.h
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
    src/Animation/FbxLoader.cpp
    src/Animation/LoadModel.cpp
    src/Animation/AnimatedModel.cpp
    src/Behavior/Graph.cpp
    src/Behavior/NodeTypes.cpp
    src/Behavior/Context.cpp
    src/Behavior/Runner.cpp
    src/Behavior/GraphValidate.cpp
    src/Behavior/GraphEdit.h
    src/Behavior/GraphEdit.cpp
    src/Behavior/GraphEditor.cpp
    src/Behavior/GraphDebug.cpp
    src/Behavior/GraphAsset.cpp
    src/Behavior/LiveGraph.cpp
    src/Behavior/Observe.cpp
    src/Behavior/ObserveState.h
    src/Behavior/Condition.h
    src/Behavior/Condition.cpp
    src/Behavior/GraphFile.cpp
    src/Audio/Audio.cpp
    src/Audio/SoundLoader.h
    src/Audio/SoundLoader.cpp
    src/Audio/AudioCodecs.cpp
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

# 音を読むライブラリ(dr_wav・stb_vorbis)も、Engine の中だけ。本体(関数の中身)は AudioCodecs.cpp に集めて、そこだけ警告を出さない(他人のコード)
target_include_directories(KREngine SYSTEM PRIVATE ${dr_libs_SOURCE_DIR})
set_source_files_properties(src/Audio/AudioCodecs.cpp PROPERTIES COMPILE_OPTIONS "/W0")

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

# ufbx も、エンジンの中だけ(PRIVATE)。公開ヘッダーには、ufbx の型を出さない
target_link_libraries(KREngine PRIVATE ufbx)

# imgui-node-editor も、エンジンの中だけ(PRIVATE)。ゲームの CMake は、変えなくてよい(imnodes は、外した = docs/Behavior.md の 15 節)
target_link_libraries(KREngine PRIVATE imgui_node_editor)

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
    xaudio2 # 音を鳴らす(XAudio2 2.9。Windows 10 以降に、最初から入っている)
    ole32   # CoInitializeEx(XAudio2 の準備)
)

# Visual Studio のソリューションエクスプローラーでフォルダ構成どおりに表示する
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_FILES})
source_group(Shaders FILES ${SHADER_FILES})
set_target_properties(DirectX-Headers DirectX-Guids imgui imgui_node_editor PROPERTIES FOLDER ThirdParty)

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
        # CONFIGURE_DEPENDS:フォルダー(Enemies など)を足したら、次のビルドで、自動で configure し直して、コピーの対象に入る
        file(GLOB asset_entries LIST_DIRECTORIES true CONFIGURE_DEPENDS "${KR_GAME_ASSETS_DIR}/*")
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

    # Debug の F1(敵の行動のツールを開く)のため、ツール KRBehaviorEditor を、exe の横にコピーする(Debug のビルドのときだけ)
    # ツールが要る DLL・Shaders は、上で、もうコピーしてある。ツールがあるのは KR_BUILD_TOOLS が ON のときだけ(ゲームは、自分の CMake で ON にする)
    # ゲームをビルドすると、ツールも一緒にビルドされる(add_dependencies)。ツール自身には、しない
    if(TARGET KRBehaviorEditor AND NOT target STREQUAL "KRBehaviorEditor" AND NOT target STREQUAL "KRAnimationEditor")
        add_dependencies(${target} KRBehaviorEditor)
        add_custom_command(TARGET ${target} POST_BUILD
            # Release では「何もしない」(cmake -E true)。配るゲームの横に、ツールを置かない
            COMMAND ${CMAKE_COMMAND} -E $<IF:$<CONFIG:Debug>,copy_if_different,true> $<TARGET_FILE:KRBehaviorEditor> $<TARGET_FILE_DIR:${target}>
            VERBATIM
        )
    endif()
endfunction()

# ゲームの exe を、1 行で作る(ゲームの CMakeLists.txt から呼ぶ)。add_executable・include の場所・リンク・DLL とアセットのコピー・
# Release の設定・Visual Studio のスタートアップを、まとめて行う。**ソースは、フォルダーの下を、自動で集める**
# (.cpp・.h・.hpp。ファイルを足したら、次のビルドで、自動で入る = CMakeLists.txt を、直さなくてよい)
#   kr_add_game(<名前> [SOURCES <フォルダー>...] [LIBRARIES <ライブラリ>...])
#     SOURCES  : ソースのフォルダー(ゲームのフォルダーからの場所。省略すると src)。#include の基点にもなる
#     LIBRARIES: エンジンのほかに、リンクする物(例 nlohmann_json::nlohmann_json)
#   例:kr_add_game(MyGame SOURCES src)
function(kr_add_game name)
    cmake_parse_arguments(PARSE_ARGV 1 KR "" "" "SOURCES;LIBRARIES")
    if(NOT KR_SOURCES)
        set(KR_SOURCES src)
    endif()

    set(game_files "")
    set(game_include_dirs "")
    foreach(dir IN LISTS KR_SOURCES)
        cmake_path(ABSOLUTE_PATH dir BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE abs_dir)
        file(GLOB_RECURSE files CONFIGURE_DEPENDS "${abs_dir}/*.cpp" "${abs_dir}/*.h" "${abs_dir}/*.hpp")
        list(APPEND game_files ${files})
        list(APPEND game_include_dirs "${abs_dir}")
        source_group(TREE "${abs_dir}" FILES ${files})   # Visual Studio で、フォルダーの形のまま見せる
    endforeach()
    if(NOT game_files)
        message(FATAL_ERROR "kr_add_game(${name}): ソースが見つかりません(SOURCES: ${KR_SOURCES})。.cpp を置いたフォルダーを、SOURCES に書いてください")
    endif()

    add_executable(${name} ${game_files})
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    # C++23 は、KREngine が PUBLIC で要求しているので、ここで書かなくても、引き継がれる。警告は、このゲームだけ
    target_compile_options(${name} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /MP>)
    target_include_directories(${name} PRIVATE ${game_include_dirs})
    target_link_libraries(${name} PRIVATE KREngine ${KR_LIBRARIES})
    kr_copy_runtime_files(${name})

    # Debug: コンソールあり(ログが見える) / Release: コンソールなし。どちらも main()(エンジンが持っている)から起動させる
    set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
    target_link_options(${name} PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)

    # Visual Studio で、最初から、このゲームを起動する対象にする(KREngine.lib を選んで「開始できません」になるのを防ぐ)
    set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT ${name})
endfunction()

# ツールの共通の部品(フォルダーを選ぶ窓。調整ツールどうしで、共有)。KR_BUILD_TOOLS が ON のときだけ。下のツールより、先に
if(KR_BUILD_TOOLS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/Common/CMakeLists.txt")
    add_subdirectory(tools/Common)
endif()

# ツール(ゲームを作るための道具。アニメーションの調整ツールなど)。KR_BUILD_TOOLS が ON のときだけ
if(KR_BUILD_TOOLS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/AnimationEditor/CMakeLists.txt")
    add_subdirectory(tools/AnimationEditor)
endif()

# 敵の行動のツール KRBehaviorEditor(ノードで、行動のグラフを作る)
if(KR_BUILD_TOOLS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/BehaviorEditor/CMakeLists.txt")
    add_subdirectory(tools/BehaviorEditor)
endif()

# サンプル(エンジンの使い方の見本・動作確認)。KR_BUILD_SAMPLES が ON のときだけ
# ツールより後に置く(kr_copy_runtime_files が、ツール KRBehaviorEditor を見つけて、サンプルの横にコピーできるように)
if(KR_BUILD_SAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/Minimal/CMakeLists.txt")
    add_subdirectory(samples/Minimal)
    add_subdirectory(samples/Animated)
    add_subdirectory(samples/Behavior)
    add_subdirectory(samples/Audio)
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
