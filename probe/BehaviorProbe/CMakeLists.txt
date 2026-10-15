# GraphEditor(部品)の動作確認用(imgui-node-editor の、ズーム・ピン・つなぐ・消す・メニュー。--graph で、好きなグラフを読む)
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

# 状態の列の、記録・照合(C2 の正解データ。C5d でも使う)。使い方:KRRecordTrace <グラフの JSON> > 結果.trace → golden/*.trace と、diff
add_executable(KRRecordTrace golden/RecordTrace.cpp)
target_include_directories(KRRecordTrace PRIVATE golden)
target_link_libraries(KRRecordTrace PRIVATE KREngine)
kr_copy_runtime_files(KRRecordTrace)
set_target_properties(KRRecordTrace PROPERTIES FOLDER Probes)
