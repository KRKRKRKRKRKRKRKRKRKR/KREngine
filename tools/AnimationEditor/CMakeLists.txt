# アニメーションの調整ツール KRAnimationEditor(骨・判定の形を、見て、調整して、保存する)
# 開いたゲームのアセットのフォルダーに、判定の形(.hitboxes.json)を保存する。ゲームは、それを読み込むだけ
add_executable(KRAnimationEditor EditorApp.cpp FolderDialog.h FolderDialog.cpp)
# ole32 / shell32 / uuid:フォルダーを選ぶダイアログ(FolderDialog.cpp。Windows の部品)に使う
target_link_libraries(KRAnimationEditor PRIVATE KREngine ole32 shell32 uuid)
kr_copy_runtime_files(KRAnimationEditor)
set_target_properties(KRAnimationEditor PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release> FOLDER Tools)
target_link_options(KRAnimationEditor PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
