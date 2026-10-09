# 使い捨ての小さな exe KRBehaviorProbe:imnodes が、エンジンの ImGui で動くかを確かめる(B3-2・B3-3)
# 確かめ終わったら、残してよい(ノードの操作の最小の見本)が、KR_BUILD_PROBES が ON のときだけビルドする
add_executable(KRBehaviorProbe ProbeApp.cpp)
# imnodes は、エンジンの中だけ(PRIVATE)で使う物。このプローブだけが、直接リンクする
target_link_libraries(KRBehaviorProbe PRIVATE KREngine imnodes)
kr_copy_runtime_files(KRBehaviorProbe)
set_target_properties(KRBehaviorProbe PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release> FOLDER Probes)
target_link_options(KRBehaviorProbe PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
