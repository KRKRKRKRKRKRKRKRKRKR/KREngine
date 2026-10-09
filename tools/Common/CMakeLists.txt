# ツールの共通の部品(KRAnimationEditor・KRBehaviorEditor が使う)
#   FolderDialog:Windows の「フォルダーを選ぶ」窓(PickFolder)。ツールの中だけ。エンジン本体は、Windows の画面に依存しない
add_library(KRToolCommon STATIC FolderDialog.h FolderDialog.cpp)
target_include_directories(KRToolCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# ole32 / shell32 / uuid:フォルダーを選ぶダイアログに使う Windows の部品
target_link_libraries(KRToolCommon PUBLIC ole32 shell32 uuid)
target_compile_features(KRToolCommon PUBLIC cxx_std_23)
target_compile_options(KRToolCommon PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4 /permissive->)
set_target_properties(KRToolCommon PROPERTIES FOLDER Tools)
