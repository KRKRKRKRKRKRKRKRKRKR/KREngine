// 状態の列を、記録する・比べる(C2 の確認用。使い捨てではなく、C5d でも使う)
// 使い方:RecordTrace <グラフの JSON>   → 標準出力に、状態が変わった所だけ(「<ステップ> <前の id> -> <あとの id>」)と、最後に集計
// 同じ入力(TraceInput.h)で、古いエンジン(版 1 の形)と、新しいエンジン(版 2)の、両方で動く(使う API は、LoadGraphFile・Runner・Context だけ)

#include "KR/Behavior/Context.h"
#include "KR/Behavior/GraphFile.h"
#include "KR/Behavior/Runner.h"

#include "TraceInput.h"

#include <cstdio>
#include <memory>
#include <set>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf("usage: RecordTrace <graph.json>\n");
        return 2;
    }
    KR::Behavior::Graph graph;
    if (KR::Behavior::LoadGraphFile(std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(argv[1]))), graph) != KR::Behavior::GraphLoadResult::Loaded)
    {
        std::printf("LOAD FAILED\n");
        return 1;
    }

    KR::Behavior::Runner runner(std::make_shared<const KR::Behavior::Graph>(std::move(graph)));
    int previous = runner.GetCurrentState().GetNodeId();
    std::printf("start %d\n", previous);
    int transitions = 0;
    std::set<int> visited{ previous };
    for (int step = 0; step < GoldenTrace::StepCount; ++step)
    {
        const GoldenTrace::Input input = GoldenTrace::InputAt(step);
        KR::Behavior::Context context;
        context.SetFloat("distanceToPlayerX", input.distanceX);
        context.SetFloat("distanceToPlayerY", input.distanceY);
        context.SetBool("attackFinished", input.attackFinished);
        runner.Update(GoldenTrace::StepSeconds, context);
        const int current = runner.GetCurrentState().GetNodeId();
        if (current != previous)
        {
            std::printf("%d %d -> %d\n", step, previous, current);
            ++transitions;
            visited.insert(current);
            previous = current;
        }
    }
    std::printf("summary transitions=%d states_visited=%d\n", transitions, static_cast<int>(visited.size()));
    return 0;
}
