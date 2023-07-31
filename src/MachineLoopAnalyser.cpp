#include "MachineLoopAnalyser.h"
#include "debug.h"
#include <queue>
#include <stack>

void MachineLoopAnalyser::clearData()
{
    doms.clear();
    backEdges.clear();
    loops.clear();
    depths.clear();
}

void MachineLoopAnalyser::computeDoms(MachineFunction *func)
{
    // 初始化
    std::set<MachineBlock *> U;
    for (auto block : func->getBlocks())
        U.insert(block);
    for (auto block : func->getBlocks())
        doms[block] = U;
    auto entry = func->getEntry();
    doms[entry].clear();
    doms[entry].insert(entry);
    // 计算支配集合
    std::queue<MachineBlock *> workList;
    for (auto block : func->getBlocks())
        if (block != entry)
            workList.push(block);
    while (!workList.empty())
    {
        auto block = workList.front();
        workList.pop();
        std::set<MachineBlock *> dom[2];
        if (block->getPreds().size() > 0)
            dom[0] = doms[block->getPreds()[0]];
        auto it = block->getPreds().begin() + 1;
        auto overPos = block->getPreds().end();
        int turn = 0;
        for (; it != overPos; it++)
        {
            intersection(doms[*it], dom[turn], dom[turn ^ 1]);
            turn ^= 1;
        }
        dom[turn].insert(block);
        if (dom[turn] != doms[block])
        {
            doms[block] = dom[turn];
            for (auto succ : block->getSuccs())
                workList.push(succ);
        }
    }
}

void MachineLoopAnalyser::lookForBackEdge(MachineFunction *func)
{
    for (auto block : func->getBlocks())
    {
        for (auto succ : block->getSuccs())
        {
            if (doms[block].find(succ) != doms[block].end())
            {
                backEdges[succ].insert(block);
            }
        }
    }
}

void MachineLoopAnalyser::computeLoops(MachineFunction *func)
{
    for (auto [d, ns] : backEdges)
    {
        loops.push_back({});
        auto &l = loops.back();
        for (auto n : ns)
        {
            std::stack<MachineBlock *> st;
            std::set<MachineBlock *> loop({n, d});
            st.push(n);
            while (!st.empty())
            {
                auto m = st.top();
                st.pop();
                for (auto pred : m->getPreds())
                {
                    if (loop.find(pred) == loop.end())
                    {
                        loop.insert(pred);
                        st.push(pred);
                    }
                }
            }
            l.insert(loop.begin(), loop.end());
        }
    }
}

void MachineLoopAnalyser::computeDepth(MachineFunction *func)
{
    auto cmp = [=](const std::set<MachineBlock *> &a, const std::set<MachineBlock *> &b)
    {
        return a.size() < b.size();
    };
    std::sort(loops.begin(), loops.end(), cmp);
    std::vector<int> loopDepth(loops.size(), 0);
    for (auto i = 0; i < loops.size(); i++)
    {
        if (loopDepth[i] == 0)
            loopDepth[i] = 1;
        for (auto j = i + 1; j < loops.size(); j++)
        {
            if (std::includes(loops[j].begin(), loops[j].end(), loops[i].begin(), loops[i].end()))
                loopDepth[j] = std::max(loopDepth[i] + 1, loopDepth[j]);
        }
    }
    std::map<MachineBlock *, std::set<int>> blk2depth;
    for (auto i = 0; i < loops.size(); i++)
        for (auto blk : loops[i])
            blk2depth[blk].insert(loopDepth[i]);
    for (auto &[blk, layers] : blk2depth)
        depths[blk] = layers.size();
}

void MachineLoopAnalyser::analyse(MachineFunction *func)
{
    clearData();
    computeDoms(func);
    lookForBackEdge(func);
    computeLoops(func);
    computeDepth(func);
}

int MachineLoopAnalyser::getDepth(MachineBlock *block)
{
    return depths[block];
}

void MachineLoopAnalyser::intersection(std::set<MachineBlock *> &a, std::set<MachineBlock *> &b, std::set<MachineBlock *> &out)
{
    out.clear();
    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), inserter(out, out.begin()));
}
