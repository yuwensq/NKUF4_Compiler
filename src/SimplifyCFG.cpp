#include "SimplifyCFG.h"
#include "Type.h"
#include <queue>

std::set<BasicBlock *> freeList;
std::set<Instruction *> freeInsts;

void SimplifyCFG::pass()
{
    for (auto it = unit->begin(); it != unit->end(); it++)
    {
        bool everChanged = removeUnreachableBlocks(*it);
        //     everChanged |= mergeEmptyReturnBlocks(*it);
        //     everChanged |= iterativelySimplifyCFG(*it);

        //     if (!everChanged)
        //         continue;

        //     if (!removeUnreachableBlocks(*it))
        //         continue;
        //     do
        //     {
        //         everChanged = iterativelySimplifyCFG(*it);
        //         everChanged |= removeUnreachableBlocks(*it);
        //     } while (everChanged);
    }
}

/// removeUnreachableBlocks - Remove blocks that are not reachable, even
/// if they are in a dead cycle.  Return true if a change was made, false
/// otherwise.
bool SimplifyCFG::removeUnreachableBlocks(Function *F)
{
    std::set<BasicBlock *> Reachable;
    std::queue<BasicBlock *> q;
    Reachable.insert(F->getEntry());
    q.push(F->getEntry());
    while (!q.empty())
    {
        auto bb = q.front();
        std::vector<BasicBlock *> preds(bb->pred_begin(), bb->pred_end());
        std::vector<BasicBlock *> succs(bb->succ_begin(), bb->succ_end());
        // 消除空的基本块，比如某些end_bb
        if (bb->empty() && bb != F->getEntry())
        {
            assert(bb->succEmpty());
            for (auto pred : preds)
            {
                auto lastInst = pred->rbegin();
                if (lastInst->isUncond())
                {
                    pred->remove(lastInst);
                    freeInsts.insert(lastInst);
                }
                else
                {
                    assert(lastInst->isCond());
                    pred->remove(lastInst);
                    freeInsts.insert(lastInst);
                    CondBrInstruction *branch = (CondBrInstruction *)(lastInst);
                    if (branch->getTrueBranch() == bb)
                        new UncondBrInstruction(branch->getFalseBranch(), pred);
                    else
                        new UncondBrInstruction(branch->getTrueBranch(), pred);
                }
                pred->removeSucc(bb);
            }
            F->remove(bb);
            freeList.insert(bb);
        }
        // 消除仅包含无条件分支的基本块。
        else if (bb != F->getEntry() && bb->begin()->getNext() == bb->end() && bb->begin()->isUncond())
        {
            assert(bb->getNumOfSucc() == 1);
            auto &succ = succs[0];
            succ->removePred(bb);
            for (auto pred : preds)
            {
                pred->removeSucc(bb);
                auto lastInst = pred->rbegin();
                if (lastInst->isCond())
                {
                    CondBrInstruction *branch = (CondBrInstruction *)(lastInst);
                    if (branch->getTrueBranch() == bb)
                        branch->setTrueBranch(succ);
                    else
                        branch->setFalseBranch(succ);
                    if (branch->getTrueBranch() == branch->getFalseBranch())
                    {
                        pred->remove(lastInst);
                        freeInsts.insert(lastInst);
                        new UncondBrInstruction(branch->getTrueBranch(), pred);
                    }
                }
                else
                {
                    assert(lastInst->isUncond());
                    freeInsts.insert(lastInst);
                    pred->remove(lastInst);
                    new UncondBrInstruction(succ, pred);
                }
                pred->addSucc(succ);
                succ->addPred(pred);
            }
            for (auto phi = succ->begin(); phi != succ->end(); phi = phi->getNext())
            {
                if (!phi->isPhi())
                    break;
                auto phinode = dynamic_cast<PhiInstruction *>(phi);
                auto &srcs = phinode->getSrcs();
                auto rep = srcs.end();
                for (auto it = srcs.begin(); it != srcs.end(); it++)
                {
                    if (it->first == bb)
                    {
                        rep = it;
                    }
                }
                if (rep != srcs.end())
                {
                    for (auto &pred : preds)
                    {
                        srcs[pred] = rep->second;
                    }
                    srcs.erase(rep->first);
                }
            }
            if (bb == F->getEntry())
                F->setEntry(succ);
            F->remove(bb);
            freeList.insert(bb);
        }
        // 如果仅有一个前驱且该前驱仅有一个后继，将基本块与前驱合并
        else if (bb->getNumOfPred() == 1 && (*(bb->pred_begin()))->getNumOfSucc() == 1 && bb != F->getEntry())
        {
            auto pred = *(bb->pred_begin());
            pred->removeSucc(bb);
            auto lastInst = pred->rbegin();
            assert(lastInst->isUncond() || (lastInst->isCond() && ((CondBrInstruction *)(lastInst))->getTrueBranch() == ((CondBrInstruction *)(lastInst))->getFalseBranch()));
            freeInsts.insert(lastInst);
            pred->remove(lastInst);
            for (auto succ : succs)
                pred->addSucc(succ);
            auto insts = std::vector<Instruction *>();
            for (auto inst = bb->begin(); inst != bb->end(); inst = inst->getNext())
                insts.push_back(inst);
            for (auto inst : insts)
            {
                bb->remove(inst);
                pred->insertBefore(inst, pred->end());
            }
            for (auto succ : succs)
            {
                succ->removePred(bb);
                succ->addPred(pred);
                for (auto phi = succ->begin(); phi != succ->end(); phi = phi->getNext())
                {
                    if (!phi->isPhi())
                        break;
                    auto phinode = dynamic_cast<PhiInstruction *>(phi);
                    auto &srcs = phinode->getSrcs();
                    auto rep = srcs.end();
                    for (auto it = srcs.begin(); it != srcs.end(); it++)
                    {
                        if (it->first == bb)
                        {
                            rep = it;
                            break;
                        }
                    }
                    if (rep != srcs.end())
                    {
                        srcs[pred] = rep->second;
                        srcs.erase(rep->first);
                    }
                }
            }
            F->remove(bb);
            freeList.insert(bb);
        }
        q.pop();
        for (auto &succ : succs)
        {
            if (!Reachable.count(succ))
            {
                q.push(succ);
                Reachable.insert(succ);
            }
        }
    }
    // 删除不可达的基本块。
    auto blocks = F->getBlockList();
    for (auto bb : blocks)
        if (!Reachable.count(bb))
        {
            F->remove(bb);
            std::vector<BasicBlock *> preds(bb->pred_begin(), bb->pred_end());
            std::vector<BasicBlock *> succs(bb->succ_begin(), bb->succ_end());
            for (auto pred : preds)
                pred->removeSucc(bb);
            for (auto succ : succs)
                succ->removePred(bb);
            freeList.insert(bb);
        }
    return true;
}

/// If we have more than one empty (other than phi node) return blocks,
/// merge them together to promote recursive block merging.
bool SimplifyCFG::mergeEmptyReturnBlocks(Function *F)
{
    bool change = false;

    BasicBlock *retBB = nullptr;

    // Scan all the blocks in the function, looking for empty return blocks.
    for (auto it = F->begin(); it != F->end(); it++)
    {
        auto bb = *it;

        // Only look at return blocks.
        auto ret = dynamic_cast<RetInstruction *>(bb->rbegin());
        if (!ret)
            continue;

        // Only look at the block if it is empty or the only other thing in it is a
        // single PHI node that is the operand to the return.
        if (ret != bb->begin())
        {
            // Check for something else in the block.
            auto I = ret->getPrev();
            if (!I->isPhi() || I != bb->begin() || ret->getOperands().size() == 0 || ret->getOperands()[0] != I->getDef())
                continue;
        }

        // If this is the first returning block, remember it and keep going.
        if (!retBB)
        {
            retBB = bb;
            continue;
        }

        // Otherwise, we found a duplicate return block.  Merge the two.
        change = true;

        // Case when there is no input to the return or when the returned values
        // agree is trivial.  Note that they can't agree if there are phis in the
        // blocks.
        if (ret->getOperands().size() == 0 ||
            ret->getOperands()[0] == retBB->rbegin()->getOperands()[0])
        {
            // BB.replaceAllUsesWith(RetBlock);
            // BB.eraseFromParent();
            continue;
        }

        // If the canonical return block has no PHI node, create one now.
        auto retBlockPHI = dynamic_cast<PhiInstruction *>(retBB->begin());
        if (!retBlockPHI)
        {
            auto inVal = retBB->rbegin()->getOperands()[0];
            retBlockPHI = new PhiInstruction(new Operand(new TemporarySymbolEntry(inVal->getType(), SymbolTable::getLabel())));
            for (auto pred = retBB->pred_begin(); pred != retBB->pred_end(); pred++)
            {
                retBlockPHI->addEdge(*pred, inVal);
            }
            retBB->rbegin()->getOperands()[0] = retBlockPHI->getDef();
        }

        // Turn BB into a block that just unconditionally branches to the return
        // block.  This handles the case when the two return blocks have a common
        // predecessor but that return different things.
        retBlockPHI->addEdge(bb, ret->getOperands()[0]);
        bb->remove(bb->rbegin());
        new UncondBrInstruction(retBB, bb);
        // BranchInst::Create(RetBlock, &BB);
    }

    return change;
}

/// Call SimplifyCFG on all the blocks in the function,
/// iterating until no more changes are made.
bool SimplifyCFG::iterativelySimplifyCFG(Function *F)
{
    // bool Changed = false;
    // bool LocalChange = true;

    // std::vector<std::pair<const BasicBlock *, const BasicBlock *>> Edges(32);
    // FindFunctionBackedges(F, Edges);
    // SmallPtrSet<BasicBlock *, 16> LoopHeaders;
    // for (unsigned i = 0, e = Edges.size(); i != e; ++i)
    //     LoopHeaders.insert(const_cast<BasicBlock *>(Edges[i].second));

    // while (LocalChange)
    // {
    //     LocalChange = false;

    //     // Loop over all of the basic blocks and remove them if they are unneeded.
    //     for (auto BBIt = F->begin(); BBIt != F->end();)
    //     {
    //         if (simplifyCFG(&*BBIt++, TTI, Options, &LoopHeaders))
    //         {
    //             LocalChange = true;
    //         }
    //     }
    //     Changed |= LocalChange;
    // }
    // return Changed;
    return false;
}
