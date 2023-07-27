#include "MachineLoopAnalyser.h"
#include "debug.h"
#include <queue>

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
}

void MachineLoopAnalyser::computeLoops(MachineFunction *func)
{
}

void MachineLoopAnalyser::analyse(MachineFunction *func)
{
    computeDoms(func);
    lookForBackEdge(func);
    computeLoops(func);
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
