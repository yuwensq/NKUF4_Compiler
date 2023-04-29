#include "MachineStraight.h"
#include "debug.h"
#include <sstream>
#include <string>

void MachineStraight::getSlimBlock()
{
    // 得到每个只有一条b指令的基本块及其后继基本块
    blk2blk.clear();
    for (auto &func : unit->getFuncs())
    {
        for (auto &blk : func->getBlocks())
        {
            if (blk->getInsts().size() == 1 && blk->getInsts()[0]->isUBranch())
            {
                blk2blk[blk->getNo()] = std::make_pair(blk, blk->getSuccs()[0]);
            }
        }
    }
}

void MachineStraight::removeSlimBlock() {
    std::string label;
    std::stringstream ss;
    MachineBlock *direct_succ;
    MachineBlock *last_succ;
    bool is_direct_succ;
    int target_no;
    // 先删除一下前驱和后继关系
    // Log("1");
    for (auto &pa : blk2blk)
    {
        // Log("2");
        auto prev = pa.second.first;
        auto succ = pa.second.second;
        prev->getParent()->RemoveBlock(prev);
        prev->removeSucc(succ);
        succ->removePred(prev);
    }
    // Log("3");
    for (auto &func : unit->getFuncs())
    {
        // Log("4");
        for (auto &blk : func->getBlocks())
        {
            // Log("5");
            for (auto &ins : blk->getInsts())
            {
                // Log("6");
                if (ins->isBranch())
                {
                    label = ins->getUse()[0]->getLabel();
                    label = label.substr(2, label.size() - 2);
                    ss.clear();
                    ss << label;
                    ss >> target_no;
                    is_direct_succ = true;
                    // Log("7");
                    while (blk2blk.find(target_no) != blk2blk.end())
                    {
                        if (is_direct_succ) {
                            direct_succ = blk2blk[target_no].first;
                            is_direct_succ = false;
                        }
                        last_succ = blk2blk[target_no].second;
                        target_no = last_succ->getNo();
                    }
                    if (!is_direct_succ) {
                        blk->removeSucc(direct_succ);
                        blk->addSucc(last_succ);
                        last_succ->addPred(blk);
                        ((BranchMInstruction *)ins)->setTarget(new MachineOperand(".L" + std::to_string(last_succ->getNo())));
                    }
                }
            }
        }
    }
}

void MachineStraight::pass()
{
    Log("伸直化开始");
    getSlimBlock();
    removeSlimBlock();
    Log("伸直化完成\n");
}
