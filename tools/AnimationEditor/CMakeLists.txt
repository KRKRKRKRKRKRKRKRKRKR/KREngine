# アニメーションの調整ツール KRAnimationEditor(骨・判定の形を、見て、調整して、保存する)
# 開いたゲームのアセットのフォルダーに、判定の形(.hitboxes.json)を保存する。ゲームは、それを読み込むだけ
# ImGuizmo(ギズモ:矢印をドラッグして、位置・大きさを変える部品)。このツールだけが使う(エンジン本体・ゲームには、入れない)
# 版は、コミットの番号で固定(2026-10-08 の最新)。ImGui の上に作られていて、imgui のターゲットにリンクする
include(FetchContent)
FetchContent_Declare(imguizmo
    URL      https://github.com/CedricGuillemet/ImGuizmo/archive/18cef5e031d8c6973d80284c67f60549fafd78c1.zip
    SOURCE_SUBDIR do_not_add_subdirectory  # ImGuizmo 自身の CMakeLists.txt は使わない(取得だけして、下で自分で定義する)
)
FetchContent_MakeAvailable(imguizmo)
add_library(imguizmo STATIC ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp)
target_include_directories(imguizmo PUBLIC ${imguizmo_SOURCE_DIR}/src)
target_link_libraries(imguizmo PUBLIC imgui)
target_compile_options(imguizmo PRIVATE /utf-8 /W0)
set_target_properties(imguizmo PROPERTIES FOLDER ThirdParty)

add_executable(KRAnimationEditor EditorApp.cpp FolderDialog.h FolderDialog.cpp)
# ole32 / shell32 / uuid:フォルダーを選ぶダイアログ(FolderDialog.cpp。Windows の部品)に使う
target_link_libraries(KRAnimationEditor PRIVATE KREngine imguizmo ole32 shell32 uuid)
kr_copy_runtime_files(KRAnimationEditor)
set_target_properties(KRAnimationEditor PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release> FOLDER Tools)
target_link_options(KRAnimationEditor PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
