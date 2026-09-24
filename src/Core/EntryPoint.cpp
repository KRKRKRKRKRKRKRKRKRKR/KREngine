#include "TenFiftyEight/Core/Application.h"
#include "TenFiftyEight/Core/Log.h"

int main()
{

    //ログの初期化
    TenFiftyEight::Log::Init();

    //アプリケーションの作成
    std::unique_ptr<TenFiftyEight::Application> app = TenFiftyEight::CreateApplication();
    
    // 初期化 → メインループ → 終了処理
    app->Run();

    return 0;
}
