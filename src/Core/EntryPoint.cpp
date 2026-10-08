#include "KR/Core/Application.h"
#include "KR/Core/Log.h"
#include "KR/Core/Version.h"

int main()
{

    //ログの初期化
    KR::Log::Init();
    KR_LOG_INFO("KR Engine {}", KR::GetVersionString()); // どの版で動いているかを、ログの最初に出す(不具合の報告のときに、役に立つ)

    //アプリケーションの作成
    std::unique_ptr<KR::Application> app = KR::CreateApplication();
    
    // 初期化 → メインループ → 終了処理
    app->Run();

    return 0;
}
