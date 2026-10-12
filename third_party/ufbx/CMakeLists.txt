# ufbx(FBX を読む。C のソース 1 本 + ヘッダー 1 つ)。エンジンの中だけで使う(KREngine が PRIVATE でリンクする)
# 取得は、最上位の CMakeLists.txt(FetchContent)。ここでは、取得した場所(UFBX_SOURCE_DIR)から、ライブラリを定義するだけ
#
# **ufbx だけは、Debug でも最適化する**(Ryusei の決定。FBX の読み込みは、重い処理で、最適化なしだと、遅い)
# このフォルダーの中だけで、Debug の設定を変える(変数は、このフォルダーの中だけに効く。エンジンのほかの部分は、変わらない)
enable_language(C)

if(MSVC)
    # /RTC1(実行時のチェック)は、最適化(/O2)と、いっしょに使えない。/Od・/Ob0(最適化なし)・/ZI(編集して続行)も、外す
    string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REGEX REPLACE "/Od|/Ob0|/JMC" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REGEX REPLACE "/ZI" "/Zi" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(APPEND CMAKE_C_FLAGS_DEBUG " /O2")
endif()

add_library(ufbx STATIC ${UFBX_SOURCE_DIR}/ufbx.c)
# SYSTEM = 他人のコードの警告は出さない
target_include_directories(ufbx SYSTEM PUBLIC ${UFBX_SOURCE_DIR})
target_compile_options(ufbx PRIVATE $<$<C_COMPILER_ID:MSVC>:/W0>)
set_target_properties(ufbx PROPERTIES FOLDER ThirdParty)
