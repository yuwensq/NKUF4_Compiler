#include "LiveVariableAnalysis.h"
#include "MachineCode.h"
#include <iostream>
#include <algorithm>
#include <chrono>

void LiveVariableAnalysis::pass(MachineUnit *unit)
{
    for (auto &func : unit->getFuncs())
    {
        computeUsePos(func);
        computeDefUse(func);
        iterate(func);
    }
}

void LiveVariableAnalysis::pass(MachineFunction *func)
{
    computeUsePos(func);
    Log("compute use pos over");
    computeDefUse(func);
    Log("compute def use over");
    iterate(func);
    Log("iterate over");
}

void LiveVariableAnalysis::computeDefUse(MachineFunction *func)
{
    for (auto &block : func->getBlocks())
    {
        for (auto inst = block->getInsts().begin(); inst != block->getInsts().end(); inst++)
        {
            auto user = (*inst)->getUse();
            std::set<MachineOperand *> temp(user.begin(), user.end());
            set_difference(temp.begin(), temp.end(),
                           def[block].begin(), def[block].end(), inserter(use[block], use[block].end()));
            auto defs = (*inst)->getDef();
            for (auto &d : defs)
                def[block].insert(all_uses[*d].begin(), all_uses[*d].end());
        }
    }
}

void LiveVariableAnalysis::iterate(MachineFunction *func)
{
    for (auto &block : func->getBlocks())
        block->live_in.clear();
    bool change;
    change = true;
    while (change)
    {
        change = false;
        for (auto &block : func->getBlocks())
        {
            block->live_out.clear();
            auto old = block->live_in;
            for (auto &succ : block->getSuccs())
            {
                block->live_out.insert(succ->live_in.begin(), succ->live_in.end());
            }
            block->live_in = use[block];
            set_difference(block->live_out.begin(), block->live_out.end(),
                           def[block].begin(), def[block].end(), inserter(block->live_in, block->live_in.end()));
            if (old != block->live_in)
                change = true;
        }
    }
}

void LiveVariableAnalysis::computeUsePos(MachineFunction *func)
{
    for (auto &block : func->getBlocks())
    {
        for (auto &inst : block->getInsts())
        {
            auto uses = inst->getUse();
            for (auto &use : uses)
                all_uses[*use].insert(use);
        }
    }
}
