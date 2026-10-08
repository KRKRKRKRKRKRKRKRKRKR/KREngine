// KR エンジンのサンプル:動くモデル(AnimatedModel)の、使い方の見本
// glTF を読んで、動きを再生して、描いて、判定の形(箱・球)を線で描く。ゲームが AnimatedModel を使うときの、そのままの雛形
// 判定の形の調整は、ツール KRAnimationEditor で行い、ここ(ゲーム)では、読み込むだけ

#include <KR/KR.h>

#include <imgui.h>

#include <memory>

using namespace DirectX;

class AnimatedApp : public KR::Application
{
public:
    AnimatedApp() : Application(KR::ApplicationDesc{ .title = "KR Engine - Animated" }) {}

protected:
    void OnInit() override
    {
        // 形・骨・動き・判定の形を読む(判定の形は、モデルの隣の「.hitboxes.json」があれば、それを使う)。最初の動きを、ループで再生しはじめる
        if (!m_Model.Load(GetRenderer(), "Test/BossObstacle/BossObstacle.glb"))
        {
            return; // 失敗は、Load がログに出している
        }

        // カメラ:モデルの中心を見て、大きさの 3 倍だけ離れる
        const KR::AnimatedModel::Bounds& bounds = m_Model.GetBounds();
        m_Center = bounds.Center();
        const float radius = bounds.Radius();
        KR::Camera camera;
        camera.SetPosition({ 0.0f, radius * 0.5f, -radius * 3.0f });
        camera.LookAt({ 0.0f, 0.0f, 0.0f });
        GetRenderer().SetCamera(camera);
    }

    void OnUpdate(float deltaTime) override
    {
        m_Model.Update(deltaTime); // 時刻を進めて、姿勢を求める
    }

    void OnRender() override
    {
        if (!m_Model.IsLoaded())
        {
            return;
        }

        // モデル全体の置き方:中心を原点に動かして、ゆっくり回す
        m_Angle += 0.01f;
        const XMMATRIX world = XMMatrixTranslation(-m_Center.x, -m_Center.y, -m_Center.z) * XMMatrixRotationY(m_Angle);

        m_Model.Draw(GetRenderer(), world); // 今の姿勢で描く

        // 判定の形を、今の姿勢で、世界の座標にして、線で描く(ゲームでは、ここで、Intersects を呼んで、当たりを調べる)
        for (const KR::WorldHitbox& shape : m_Model.GetWorldHitboxes(world))
        {
            if (shape.shape == KR::HitShape::Box)
            {
                GetRenderer().DrawWireBox(shape.box, { 0.2f, 1.0f, 0.2f, 1.0f });
            }
            else
            {
                GetRenderer().DrawWireSphere(shape.sphere, { 0.2f, 1.0f, 0.2f, 1.0f });
            }
        }
    }

    void OnImGui() override
    {
        ImGui::Begin("Animated");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();
    }

    void OnShutdown() override
    {
        m_Model.Unload(GetRenderer());
    }

private:
    KR::AnimatedModel m_Model;
    XMFLOAT3 m_Center = {};
    float m_Angle = 0.0f;
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<AnimatedApp>();
}
