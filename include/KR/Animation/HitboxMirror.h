#pragma once

#include "KR/Animation/Hitbox.h"

#include <DirectXMath.h>

#include <optional>
#include <string>

namespace KR
{
    // 判定の形を、左右反転して、対になる骨に付け直す(調整ツールの「左右反転して貼り付け」で使う)
    //
    // 反転の面は、モデルの座標の X = 0(モデルを正面から見て、左右が対称になる面)。
    // 形は、骨の中の座標で持っているので、次の順に直す:
    //   ① 形の位置・向きを、モデルの座標に直す(元の骨の行列を掛ける)
    //   ② X を反転する
    //   ③ 対の骨の中の座標に戻す(対の骨の行列の逆を掛ける)
    //
    // 骨の行列は、どちらも「同じ姿勢」(普通は、基準姿勢)のものを渡す。モデル全体の行列(置き方)は、含めない
    //   sourceBoneWorld : 元の骨の、モデルの座標での行列(Skeleton の基準姿勢なら、EvaluatePose の結果の、その骨)
    //   targetBoneWorld : 対の骨の、同じ姿勢での行列
    //
    // 箱の向き:X を反転すると、座標の右手・左手が入れ替わる(向きが、鏡映しになる)。
    // 回転では、鏡映しは表せないので、箱の軸のうち、1 本(X の軸)の向きを、逆にして、回転に直す。
    // 箱は、軸の向きを逆にしても、同じ形(面の位置・大きさは、同じ)なので、見た目は、鏡映しのままで、変わらない。
    // 大きさ(半分の大きさ・半径)は、反転しても変わらない。名前は、そのまま(付け直す側が、決める)。
    // 戻り値の jointIndex は targetJoint になる
    Hitbox MirrorHitbox(const Hitbox& source, const DirectX::XMFLOAT4X4& sourceBoneWorld,
                        int targetJoint, const DirectX::XMFLOAT4X4& targetBoneWorld);

    // 対になる骨の名前(左 ⇔ 右)。名前の決まりは、次の順に調べる(最初に当てはまった物):
    //   ① 終わりが .L / .R・_L / _R(小文字の .l / _r も)。Blender の連番(.L.001)があっても、よい
    //   ② Left / Right(left / right・LEFT / RIGHT も)を含む。例:LeftArm ⇔ RightArm・mixamorig:LeftHand ⇔ mixamorig:RightHand
    //   ③ 左 / 右 を含む。例:左腕 ⇔ 右腕
    // どれにも当てはまらなければ nullopt(中央の骨など。ツールでは、ユーザーが選ぶ)
    std::optional<std::string> MirrorBoneName(const std::string& name);

    // 骨 joint の、対になる骨(MirrorBoneName の名前の骨が、Skeleton の中にあれば、その番号)。無ければ nullopt
    std::optional<int> FindMirrorJoint(const Skeleton& skeleton, int joint);
}
