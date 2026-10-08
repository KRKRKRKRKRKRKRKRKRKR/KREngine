#pragma once

#include "KR/Animation/AnimationClip.h"
#include "KR/Animation/Hitbox.h"
#include "KR/Animation/Skeleton.h"
#include "KR/Renderer/Mesh.h"
#include "KR/Renderer/Renderer.h"
#include "KR/Renderer/Texture.h"

#include <DirectXMath.h>

#include <filesystem>
#include <string>
#include <vector>

namespace KR
{
    // 動くモデル 1 体を、まとめて扱う窓口(骨・動き・判定の形・描画)。ゲームは、これだけ使えばよい
    //
    //   AnimatedModel boss;
    //   boss.Load(GetRenderer(), "Boss/Boss.glb");     // OnInit:形・骨・動き・判定の形を読む(最初の動きを、ループで再生しはじめる)
    //   boss.Play("Walk");                              // 動きを選ぶ(名前か番号。ループしないなら、第 2 引数 false)
    //   boss.Update(deltaTime);                         // OnUpdate:時刻を進めて、姿勢を求める
    //   boss.Draw(GetRenderer(), world);                // OnRender:骨つきのメッシュを描く
    //   boss.GetWorldHitboxes(world);                   // 当たり判定:今の姿勢の、世界の形(箱・球)。Intersects で調べる
    //   boss.Unload(GetRenderer());                     // OnShutdown
    //
    // 中身は、LoadGltf・EvaluatePose・ComputeSkinMatrices・EvaluateHitboxes などの部品を呼ぶだけ(部品は、単独でも使える)
    class AnimatedModel
    {
    public:
        // メッシュを囲む箱(モデルの座標。基準姿勢)。カメラを置いたり、中心を決めたりするのに使う
        struct Bounds
        {
            DirectX::XMFLOAT3 min = { 0.0f, 0.0f, 0.0f };
            DirectX::XMFLOAT3 max = { 0.0f, 0.0f, 0.0f };

            DirectX::XMFLOAT3 Center() const { return { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f }; }
            // 中心から、いちばん遠い角までの距離(箱の対角線の半分)
            float Radius() const;
        };

        // ---- 読み込み・片づけ ----

        // glTF(Assets/Models からの相対パス。例 "Boss/Boss.glb")を読む。失敗したら、エラーを出して false
        // 判定の形は、同じ場所の「<名前>.hitboxes.json」があれば読み、無ければ(読めなければ)、骨ごとの既定の形を作る
        bool Load(Renderer& renderer, const std::filesystem::path& relativePath);

        // GPU の形を返して、空にする(Renderer を捨てる前に、呼ぶ)
        void Unload(Renderer& renderer);

        bool IsLoaded() const { return m_Mesh.IsValid(); }

        // ---- 再生 ----

        size_t GetClipCount() const { return m_Clips.size(); }
        const std::string& GetClipName(size_t index) const { return m_Clips[index].name; }

        // 動きを選んで、最初から再生する(再生中の動きを選び直しても、最初に戻る)。見つからなければ警告して false(今の再生は、そのまま)
        bool Play(size_t clipIndex, bool loop = true);
        bool Play(const std::string& clipName, bool loop = true);

        // 動きをやめて、基準姿勢(動かない姿勢)に戻す
        void Stop();

        void Pause(bool paused = true) { m_Paused = paused; }
        bool IsPaused() const { return m_Paused; }

        void SetSpeed(float speed) { m_Speed = speed; } // 1 = 等倍・0 = 止まる(負の値は、逆再生)
        float GetSpeed() const { return m_Speed; }

        void SetLooping(bool loop) { m_Loop = loop; }
        bool IsLooping() const { return m_Loop; }

        // 再生中の動きの番号(動きが無い・Stop したあとは -1)
        int GetCurrentClip() const { return m_CurrentClip; }
        float GetDuration() const; // 再生中の動きの長さ(秒)。動きが無ければ 0

        float GetTime() const { return m_Time; } // 動きの、今の時刻(秒)
        void SetTime(float time);                // 時刻を決めて、姿勢も求め直す(止めているときに、姿勢を確かめるときなど)

        // ループしない動きが、最後まで再生された(最後の姿勢で止まっている)
        bool IsFinished() const { return m_Finished; }

        // 毎フレーム 1 回:時刻を進めて、姿勢を求める(Pause 中は、時刻を進めない)
        void Update(float deltaTime);

        // ---- 描く ----

        // 今の姿勢で、メッシュを描く(骨つきは DrawSkinnedMesh・骨なしは DrawMesh を、自動で選ぶ)。world は、モデル全体の置き方
        void Draw(Renderer& renderer, const DirectX::XMMATRIX& world, TextureHandle texture = {}) const;

        // ---- 骨・姿勢(骨の表示・判定の編集などに使う)----

        const Skeleton& GetSkeleton() const { return m_Skeleton; }
        // 今の姿勢:各骨の、世界の行列(モデルの座標。Skeleton::joints と同じ並び)
        const std::vector<DirectX::XMFLOAT4X4>& GetPose() const { return m_Pose; }
        const Bounds& GetBounds() const { return m_Bounds; }

        // ---- 判定の形 ----

        // 骨にくっついた判定の形(編集できる。ImGui で調整して、SaveHitboxes で保存する)
        std::vector<Hitbox>& GetHitboxes() { return m_Hitboxes; }
        const std::vector<Hitbox>& GetHitboxes() const { return m_Hitboxes; }

        // 今の姿勢での、世界の形(hitboxes と同じ数・同じ並び)。world は、Draw に渡す行列と同じ物
        std::vector<WorldHitbox> GetWorldHitboxes(const DirectX::XMMATRIX& world) const;

        // 判定の形を、ファイルに保存する(「<名前>.hitboxes.json」。Debug では元の Assets フォルダー)。失敗したら false
        bool SaveHitboxes() const;
        // ファイルから、読み直す(ファイルが無い・壊れていれば、骨ごとの既定の形に戻す)。ファイルから読めたら true
        bool ReloadHitboxes();

    private:
        // 時刻から、姿勢と、骨の行列の表(パレット)を求める
        void UpdatePose();

        std::filesystem::path m_ModelPath; // Load に渡された、相対パス

        MeshHandle m_Mesh;
        bool m_Skinned = false; // 骨つきのメッシュか(Draw が、DrawSkinnedMesh を使うか)
        Skeleton m_Skeleton;
        std::vector<AnimationClip> m_Clips;
        std::vector<Hitbox> m_Hitboxes;
        Bounds m_Bounds;

        // 再生の状態
        int m_CurrentClip = -1;
        float m_Time = 0.0f;
        float m_Speed = 1.0f;
        bool m_Loop = true;
        bool m_Paused = false;
        bool m_Finished = false;

        // 今の姿勢(UpdatePose が求める)
        std::vector<DirectX::XMFLOAT4X4> m_Pose;
        std::vector<DirectX::XMFLOAT4X4> m_SkinMatrices; // 骨つきメッシュを描くための、骨の行列の表(逆バインド × 今の姿勢)
    };
}
