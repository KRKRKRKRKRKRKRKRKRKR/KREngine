#include "KR/Core/Application.h"
#include "KR/Core/Log.h"

int main()
{

    //ログの初期化
    KR::Log::Init();

    //アプリケーションの作成
    std::unique_ptr<KR::Application> app = KR::CreateApplication();
    
    // 初期化 → メインループ → 終了処理
    app->Run();

    return 0;
}
