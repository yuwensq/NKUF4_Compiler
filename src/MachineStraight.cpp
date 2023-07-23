#include "MachineStraight.h"
#include "debug.h"
#include <sstream>
#include <string>

// #define PRINTLOG

void MachineStraight::getSlimBlock()
{
    // 得到每个只有一条b指令的基本块及其后继基本块
    for (auto &func : unit->getFuncs())
    {
        for (auto &blk : func->getBlocks())
        {
            if (blk->getInsts().size() == 1 && blk->getInsts()[0]->isUBranch())
            {
                // 先不处理entry块
                if (blk == func->getEntry())
                    continue;
                blk2blk[blk->getNo()] = std::make_pair(blk, blk->getSuccs()[0]);
            }
        }
    }
}

void MachineStraight::removeSlimBlock()
{
    std::string label;
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
                    target_no = atoi(label.c_str());
                    is_direct_succ = true;
                    // Log("7");
                    while (blk2blk.find(target_no) != blk2blk.end())
                    {
                        if (is_direct_succ)
                        {
                            direct_succ = blk2blk[target_no].first;
                            is_direct_succ = false;
                        }
                        last_succ = blk2blk[target_no].second;
                        target_no = last_succ->getNo();
                    }
                    if (!is_direct_succ)
                    {
                        blk->removeSucc(direct_succ);
                        blk->addSucc(last_succ);
                        last_succ->addPred(blk);
                        ((BranchMInstruction *)ins)->setTarget(new MachineOperand(".L" + std::to_string(last_succ->getNo())));
                    }
                }
            }
        }
        auto entry = func->getEntry();
        if (entry->getInsts().size() == 1 && entry->getInsts()[0]->isUBranch())
        {
            auto succ = entry->getSuccs()[0];
            entry->removeSucc(succ);
            succ->removePred(entry);
            func->RemoveBlock(entry);
            func->RemoveBlock(succ);
            func->InsertFront(succ);
            func->setEntry(succ);
        }
    }
}

void MachineStraight::getJunctions()
{
    for (auto &func : unit->getFuncs())
    {
        for (auto &blk : func->getBlocks())
        {
            if (blk->getPreds().size() == 1 && blk->getPreds()[0]->getSuccs().size() == 1)
            {
                Assert((*blk->getPreds()[0]->rbegin())->isBranch(), "最后一条指令应该是无条件跳转");
                junctions.insert(blk);
            }
        }
    }
}

void MachineStraight::mergeJunctions()
{
    std::unordered_set<MachineBlock *> color;
    for (auto &blk : junctions)
    {
        if (color.count(blk))
            continue;
        auto headBlk = blk;
        while (junctions.count(headBlk))
            headBlk = headBlk->getPreds()[0];
        auto junctionBlk = headBlk->getSuccs()[0];
        while (junctions.count(junctionBlk))
        {
            color.insert(junctionBlk);
            // 合并两个基本块
            // 先删除最后一个b指令
            auto lastBranch = *headBlk->rbegin();
            headBlk->eraseInst(lastBranch);
            // 把第二个块的指令放到第一个块里
            for (auto &ins : junctionBlk->getInsts())
            {
                headBlk->InsertInst(ins);
                ins->setParent(headBlk);
            }
            // 修改第一个块的后继关系
            headBlk->getSuccs().clear();
            headBlk->getSuccs().assign(junctionBlk->getSuccs().begin(), junctionBlk->getSuccs().end());
            for (auto &succs : headBlk->getSuccs())
            {
                succs->removePred(junctionBlk);
                succs->addPred(headBlk);
            }
            // 最后删除这个基本块
            junctionBlk->getParent()->RemoveBlock(junctionBlk);
            junctionBlk = junctionBlk->getSuccs().size() > 0 ? junctionBlk->getSuccs()[0] : nullptr;
        }
    }
}

void MachineStraight::pass()
{
    blk2blk.clear();
    junctions.clear();
#ifdef PRINTLOG
    Log("伸直化开始");
#endif
    getSlimBlock();
    removeSlimBlock();
#ifdef PRINTLOG
    Log("伸直化阶段1完成");
#endif
    getJunctions();
    mergeJunctions();
#ifdef PRINTLOG
    Log("伸直化完成\n");
#endif
}

// 这个优化是啥吧，就是比如末尾是一条b指令的基本块，可以把b的目标基本块紧挨着这个基本块
// 打印出来，然后把b指令删掉
void MachineStraight::pass2()
{
    for (auto func : unit->getFuncs())
    {
        std::map<int, MachineBlock *> no2blk;
        for (auto blk : func->getBlocks())
            no2blk[blk->getNo()] = blk;
        std::vector<MachineBlock *> tmp_blks;
        tmp_blks.assign(func->getBlocks().begin(), func->getBlocks().end());
        func->getBlocks().clear();
        for (auto i = 0; i < tmp_blks.size(); i++)
        {
            if (no2blk.find(tmp_blks[i]->getNo()) == no2blk.end())
                continue;
            func->getBlocks().push_back(tmp_blks[i]);
            no2blk.erase(tmp_blks[i]->getNo());
            auto lastInst = tmp_blks[i]->getInsts().back();
            if (lastInst->isUBranch())
            {
                auto label = lastInst->getUse()[0]->getLabel();
                label = label.substr(2, label.size() - 2);
                auto succNo = atoi(label.c_str());
                if (no2blk.find(succNo) != no2blk.end())
                {
                    func->getBlocks().push_back(no2blk[succNo]);
                    no2blk.erase(succNo);
                    tmp_blks[i]->eraseInst(lastInst);
                }
            }
        }
    }
}
