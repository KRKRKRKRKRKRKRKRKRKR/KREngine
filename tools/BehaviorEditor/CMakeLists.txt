# 敵の行動の調整ツール KRBehaviorEditor(状態と条件を、ノードでつないで、行動のグラフを作る)
# 作ったグラフは、ゲームの Assets/Enemies/<名前>.json に保存する。ゲームは、それを読み込むだけ(KR::Behavior::LoadGraph)
# imnodes は、エンジンの中(GraphEditor)だけ。このツールは、KREngine にリンクするだけでよい
add_executable(KRBehaviorEditor EditorApp.cpp)
target_link_libraries(KRBehaviorEditor PRIVATE KREngine)
kr_copy_runtime_files(KRBehaviorEditor)
set_target_properties(KRBehaviorEditor PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release> FOLDER Tools)
target_link_options(KRBehaviorEditor PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
