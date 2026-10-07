# 最小のサンプル KRMinimal(ウィンドウ + 回るキューブ + ImGui の FPS)
# 新しいゲームの CMakeLists.txt の、見本にもなる:実行ファイルを作って、KREngine をリンクして、kr_copy_runtime_files を呼ぶ
add_executable(KRMinimal MinimalApp.cpp)
target_link_libraries(KRMinimal PRIVATE KREngine)

# 実行に必要なファイル(DXC の DLL・エンジンのシェーダー・あればゲームのアセット)を、exe の横にコピーする
kr_copy_runtime_files(KRMinimal)

# Debug: コンソールあり(ログが見える) / Release: コンソールなし。どちらも main() から起動させるため、Release ではエントリーを明示する
set_target_properties(KRMinimal PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release> FOLDER Samples)
target_link_options(KRMinimal PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
