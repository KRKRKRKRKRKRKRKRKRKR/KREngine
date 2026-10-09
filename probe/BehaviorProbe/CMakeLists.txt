# 使い捨ての小さな exe KRBehaviorProbe:imnodes が、エンジンの ImGui で動くかを確かめる(B3-2・B3-3)
# 確かめ終わったら、残してよい(ノードの操作の最小の見本)が、KR_BUILD_PROBES が ON のときだけビルドする
add_executable(KRBehaviorProbe ProbeApp.cpp)
# imnodes は、エンジンの中だけ(PRIVATE)で使う物。このプローブだけが、直接リンクする
target_link_libraries(KRBehaviorProbe PRIVATE KREngine imnodes)
kr_copy_runtime_files(KRBehaviorProbe)
set_target_properties(KRBehaviorProbe PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release> FOLDER Probes)
target_link_options(KRBehaviorProbe PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)

# GraphEditor(部品)の動作確認用。imnodes には、直接さわらない(GraphEditor の中だけ)
add_executable(KRGraphEditorProbe GraphEditorProbe.cpp)
target_link_libraries(KRGraphEditorProbe PRIVATE KREngine)
kr_copy_runtime_files(KRGraphEditorProbe)
set_target_properties(KRGraphEditorProbe PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release> FOLDER Probes)
target_link_options(KRGraphEditorProbe PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)

# DrawGraphDebug(動いている敵の今の状態を光らせる表示)の動作確認用
add_executable(KRDebugDrawProbe DebugDrawProbe.cpp)
target_link_libraries(KRDebugDrawProbe PRIVATE KREngine)
kr_copy_runtime_files(KRDebugDrawProbe)
set_target_properties(KRDebugDrawProbe PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release> FOLDER Probes)
target_link_options(KRDebugDrawProbe PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
