#include "GVN.h"
#include <vector>
#include <unordered_map>
#include <queue>
#include <set>

using namespace std;

void GlobalValueNumbering::pass()
{
    for (auto it = unit->begin(); it != unit->end(); it++)
    {
        preprocess(*it);
    }
}

bool isSameRHS(Instruction *a, Instruction *b)
{
    if (!(a->isBinary() && b->isBinary()))
        return false;
    if (a->getOpCode() != b->getOpCode())
        return false;
    auto a1 = a->getOperands()[1], a2 = a->getOperands()[2];
    auto b1 = b->getOperands()[1], b2 = b->getOperands()[2];
    if (a1 == b1)
    {
        if (a2 == b2)
            return true;
        if (a2->getType()->isConst() && b2->getType()->isConst())
        {
            if (((ConstantSymbolEntry *)a2->getEntry())->getValue() == ((ConstantSymbolEntry *)b2->getEntry())->getValue())
                return true;
        }
    }
    else if (a1 == b2)
    {
        if (a2 == b1)
            return true;
        if (a2->getType()->isConst() && b1->getType()->isConst())
        {
            if (((ConstantSymbolEntry *)a2->getEntry())->getValue() == ((ConstantSymbolEntry *)b1->getEntry())->getValue())
                return true;
        }
    }
    else if (a1->getType()->isConst())
    {
        if (a2 == b2)
        {
            if (a1->getType()->isConst() && b1->getType()->isConst())
            {
                if (((ConstantSymbolEntry *)a1->getEntry())->getValue() == ((ConstantSymbolEntry *)b1->getEntry())->getValue())
                    return true;
            }
        }
        else if (a2 == b1)
        {
            if (a1->getType()->isConst() && b2->getType()->isConst())
            {
                if (((ConstantSymbolEntry *)a1->getEntry())->getValue() == ((ConstantSymbolEntry *)b2->getEntry())->getValue())
                    return true;
            }
        }
    }
    return false;
}

void GlobalValueNumbering::preprocess(Function *func)
{
    vector<BasicBlock *> blocks;
    unordered_map<BasicBlock *, bool> visited;
    queue<BasicBlock *> q;
    q.push(func->getEntry());
    visited[func->getEntry()] = true;
    while (!q.empty())
    {
        auto BB = q.front();
        q.pop();
        blocks.push_back(BB);
        for (auto inst = BB->begin(); inst != BB->end(); inst = inst->getNext())
        {
            assignRanks(inst);
            if (inst->isAdd() && inst->getOperands()[1]->getEntry()->isTemporary() && inst->getOperands()[2]->toStr() == "0")
            {
                trivial_worklist.push_back(inst);
            }
            else if (inst->isBinary())
            {
                LCTs[BB][inst].push_back(inst);
            }
        }
        for (auto succ = BB->succ_begin(); succ != BB->succ_end(); succ++)
        {
            if (!visited[*succ])
            {
                q.push(*succ);
                visited[*succ] = true;
            }
            else
            {
                auto &loopheader = *succ;
                loopheaders.insert(loopheader);
                // 1. find entrance
                assert(loopheader->getNumOfPred() == 1);
                auto test = *(loopheader->pred_begin());
                // 2. add a landing pad
                auto landingpad = new BasicBlock(func);
                landingpads.insert(landingpad);
                loopheader->removePred(test);
                test->removeSucc(loopheader);
                loopheader->addPred(landingpad);
                landingpad->addPred(test);
                // 3. find exit
                auto next = *(test->succ_begin());
                assert(next != landingpad);
                // 4. add a virtual edge
                virtualEdge[landingpad].insert(next);
                virtualREdge[next].insert(landingpad);
                // auto &loopheader = *succ;
                // unordered_map<BasicBlock *, bool> backwards;
                // queue<BasicBlock *> loopbody;
                // q.push(BB);
                // visited[BB] = true;
                // visited[loopheader] = true;
                // while (!loopbody.empty())
                // {
                //     auto cur = loopbody.front();
                //     loopbody.pop();
                //     for (auto pred = cur->pred_begin(); pred != cur->pred_end(); pred++)
                //     {
                //         if (*pred == loopheader) // TODO：or a previously visited node
                //             break;
                //         if (!backwards[*pred])
                //         {
                //             loopbody.push(*pred);
                //             backwards[*pred] = true;
                //         }
                //     }
                // }
                // auto &next = loopheader->getSucc();
                // assert(next.size() == 2);
                // if (backwards[next[0]])
                //     loopexits.push_back({loopheader, next[0]});
                // else
                //     loopexits.push_back({loopheader, next[1]});
            }
        }
    }

    // Modify the Graph
    // DELETED!!~~~~

    removeTrivialAssignments();
}

void GlobalValueNumbering::assignRanks(Instruction *inst)
{
    if (inst->isPhi())
    {
        int rank = 0;
        for (auto &[block, op] : ((PhiInstruction *)inst)->getSrcs())
        {
            if (ranks.count(op) && ranks[op] > rank)
            {
                rank = ranks[op];
            }
        }
        ranks[inst->getDef()] = rank;
        return;
    }
    else if (inst->isBinary())
    {
        auto &ops = inst->getOperands();
        ranks[inst->getDef()] = max(ranks[ops[2]], ranks[ops[1]]) + 1;
        return;
    }
}

void GlobalValueNumbering::removeTrivialAssignments()
{
    for (auto &&inst : trivial_worklist)
    {
        auto &ops = inst->getOperands();
        auto &A = ops[0], &B = ops[1];
        if (A->getUse().size() < B->getUse().size())
        {
            for (auto &&use_inst : A->getUse())
            {
                use_inst->replaceUse(A, B);
                if (use_inst->isBinary())
                    LCTs[use_inst->getParent()][use_inst].push_back(use_inst);
                /*else if (use_inst->isPhi())
                {
                    bool allsame = true;
                    for (auto &&[block, v] : ((PhiInstruction*)use_inst)->getSrcs())
                    {
                        if (v != B)
                        {
                            allsame = false;
                            break;
                        }
                    }
                    if (allsame)
                    {
                        // Phi => Def <- B
                        // eliminate new trivial
                    }
                }*/
            }
        }
        else
        {
            for (auto &&use_inst : B->getUse())
            {
                use_inst->replaceUse(B, A);
                if (use_inst->isBinary())
                    LCTs[use_inst->getParent()][use_inst].push_back(use_inst);
            }
        }
        inst->getParent()->remove(inst);
        delete inst;
    }
    eliminateLocalRedundancies();
}

void GlobalValueNumbering::eliminateLocalRedundancies()
{
    for (auto &&[block, list] : LCTs)
    {
        for (auto inst = block->begin(); inst != block->end(); inst = inst->getNext())
        {
            if (inst->isBinary())
            {
                if (list[inst].size() > 1)
                {
                    for (auto &&elim : list[inst])
                    {
                        if (elim == inst)
                            continue;
                        auto trivial = new BinaryInstruction(
                            BinaryInstruction::ADD,
                            elim->getDef(),
                            inst->getDef(),
                            new Operand(new ConstantSymbolEntry(elim->getDef()->getType(), 0)));
                        block->insertBefore(trivial, elim);
                        trivial_worklist.push_back(trivial);
                        block->remove(elim);
                        delete elim;
                    }
                }
            }
        }
    }
    if (!trivial_worklist.empty())
        removeTrivialAssignments();
}

void GlobalValueNumbering::eliminateRedundancies(Function *func)
{
    func->getReversedTopsort().swap(rtop);
    for (cur_rank = 0; cur_rank < max_rank; cur_rank++)
    {
        // for each node n (in reverse topsort order) do
        for (auto &BB : rtop)
        {
            if (landingpads.count(BB))
            {
                moveComputationsFromSuccessors(BB);
                moveComputationsIntoPad(BB);
                identifyMovableComputations(BB);
            }
            else if (loopheaders.count(BB))
            {
                moveComputationsFromSuccessors(BB);
                identifyMovableComputations2(BB);
            }
            else
            {
                moveComputationsFromSuccessors(BB);
                identifyMovableComputations(BB);
            }
        }

        for (auto it = rtop.rbegin(); it != rtop.rend(); it++)
        {
            vector<Instruction *> del_list;
            for (auto inst = (*it)->begin(); inst != (*it)->end(); inst = inst->getNext())
            {
                if (!inst->isBinary())
                    continue;
                auto &ops = inst->getOperands();
                if (ranks[inst->getDef()] > max(ranks[ops[2]], ranks[ops[1]]) + 1)
                {
                    for (auto &use_inst : inst->getDef()->getUse())
                    {
                        if (use_inst->getParent() != inst->getParent())
                            continue;
                        if (ranks[use_inst->getDef()] < cur_rank + 1)
                            ranks[use_inst->getDef()] = cur_rank + 1;
                    }
                }
                eliminateGlobalRedundancies(del_list, inst);
            }
            for (auto &&inst : del_list)
            {
                inst->getParent()->remove(inst);
                delete inst;
            }
        }
    }
}

void GlobalValueNumbering::moveComputationsFromSuccessors(BasicBlock *block)
{
    auto succs = block->getSucc();
    // auto j_inst = block->rbegin(); 能不能这么写，插到跳转语句前？
    if (succs.size() == 1)
    {
        // everything in the MCT of the outedge will be moved into the current node.
        auto j_inst = block->rbegin();
        for (auto &&inst : MCTs[{block, succs[0]}])
        {
            block->insertBefore(inst, j_inst);
            LCTs[block][inst].push_back(inst);
        }
        MCTs[{block, succs[0]}].clear();
    }
    else if (succs.size() == 2)
    {
        auto j_inst = block->rbegin();
        auto &m1 = MCTs[{block, succs[0]}];
        auto &m2 = MCTs[{block, succs[1]}];
        auto &o1 = MCTps[{block, succs[0]}];
        auto &o2 = MCTps[{block, succs[1]}];
        unordered_set<Instruction *> removal_list;
        for (auto &&inst : m1)
        {
            // rhs of inst in m2
            for (auto it = m2.begin(); it != m2.end(); it++)
            {
                auto &inst_ = *it;
                if (isSameRHS(inst, inst_))
                {
                    block->insertBefore(inst_, j_inst);
                    block->insertBefore(
                        new BinaryInstruction(
                            BinaryInstruction::ADD,
                            inst->getDef(),
                            inst_->getDef(),
                            new Operand(new ConstantSymbolEntry(inst->getDef()->getType(), 0))),
                        j_inst);
                    LCTs[block][inst_].push_back(inst_);
                    // TODO：需不需要 改def-use？

                    auto &original = o2[it - m2.begin()];
                    if (ranks[original->getDef()] < cur_rank)
                        ranks[original->getDef()] = cur_rank;

                    removal_list.insert(inst);
                    m2.erase(it);
                    break;
                }
            }
        }
        for (auto it = m1.begin(); it != m1.end();)
        {
            if (removal_list.count(*it))
            {
                auto &original = o1[it - m1.begin()];
                if (ranks[original->getDef()] < cur_rank)
                    ranks[original->getDef()] = cur_rank;
                delete *it;
                it = m1.erase(it);
            }
            else
            {
                it++;
            }
        }
    }
}

void GlobalValueNumbering::identifyMovableComputations(BasicBlock *block)
{
    // TODO：virtual edge
    for (auto inst = block->begin(); inst != block->end(); inst = inst->getNext())
    {
        if (inst->isPhi())
            break;
        if (!inst->isBinary())
            continue;
        auto &ops = inst->getOperands();
        if (ranks[ops[0]] != cur_rank)
            continue;
        // TODO：有一个op是之前BB中的计算结果怎么处理？目前认为符合 movable 条件之一
        auto inst1 = ops[1]->getDef(), inst2 = ops[2]->getDef();
        if (inst1->isPhi() && inst2->isPhi())
        {
            auto &srcs1 = ((PhiInstruction *)inst1)->getSrcs();
            auto &srcs2 = ((PhiInstruction *)inst2)->getSrcs();
            assert(block->getNumOfPred() == srcs1.size());
            assert(block->getNumOfPred() == srcs2.size());
            for (auto &&pred : block->getPred())
            {
                MCTs[{pred, block}].push_back(
                    new BinaryInstruction(
                        inst->getOpCode(),
                        new Operand(new TemporarySymbolEntry(inst->getDef()->getType(), SymbolTable::getLabel())),
                        srcs1[pred], srcs2[pred]));
                MCTps[{pred, block}].push_back(inst);
            }
        }
        else if (inst1->isPhi())
        {
            if (inst2->getParent() == block)
                continue;
            for (auto &&[pred, op] : ((PhiInstruction *)inst1)->getSrcs())
            {
                MCTs[{pred, block}].push_back(
                    new BinaryInstruction(
                        inst->getOpCode(),
                        new Operand(new TemporarySymbolEntry(inst->getDef()->getType(), SymbolTable::getLabel())),
                        op, ops[2]));
                MCTps[{pred, block}].push_back(inst);
            }
        }
        else if (inst2->isPhi())
        {
            if (inst1->getParent() == block)
                continue;
            for (auto &&[pred, op] : ((PhiInstruction *)inst2)->getSrcs())
            {
                MCTs[{pred, block}].push_back(
                    new BinaryInstruction(
                        inst->getOpCode(),
                        new Operand(new TemporarySymbolEntry(inst->getDef()->getType(), SymbolTable::getLabel())),
                        ops[1], op));
                MCTps[{pred, block}].push_back(inst);
            }
        }
        else
        {
            if (inst1->getParent() == block || inst2->getParent() == block)
                continue;
            for (auto &&pred : block->getPred())
            {
                MCTs[{pred, block}].push_back(
                    new BinaryInstruction(
                        inst->getOpCode(),
                        new Operand(new TemporarySymbolEntry(inst->getDef()->getType(), SymbolTable::getLabel())),
                        ops[1], ops[2]));
                MCTps[{pred, block}].push_back(inst);
            }
        }
    }
}

void GlobalValueNumbering::identifyMovableComputations2(BasicBlock *block)
{
    for (auto inst = block->begin(); inst != block->end(); inst = inst->getNext())
    {
        if (inst->isPhi())
            break;
        if (!inst->isBinary())
            continue;
        auto &ops = inst->getOperands();
        if (ranks[ops[0]] != cur_rank)
            continue;
        moveComputationsOutOfALoop(block);
    }
}

bool GlobalValueNumbering::questionPropagation(Instruction *inst)
{
    // auto &ops = inst->getOperands();
    // auto inst1 = ops[1]->getDef(), inst2 = ops[2]->getDef();
    // auto block = inst->getParent();
    // if (!inst1->isPhi() && inst1->getParent() == block)
    //     return false;
    // if (!inst2->isPhi() && inst2->getParent() == block)
    //     return false;

    // questions are propagated to predecessors of n
    if (!qpLocalSearch(inst))
        return false;

    vector<Instruction *> may_list;
    if (!qpGlobalSearch(may_list, inst))
        return false;

    return true;
}

bool GlobalValueNumbering::qpLocalSearch(Instruction *inst)
{
    auto block = inst->getParent();
    if (loopheaders.count(block))
        return false;
    auto it = find(rtop.begin(), rtop.end(), block);
    map<BasicBlock *, Instruction *> questions;
    questions[block] = inst;
    for (; it != rtop.end(); it++)
    {
        if (loopheaders.count(*it)) // 1
            return false;
        if (questions.find(*it) == questions.end())
            continue;
        auto inst = questions[*it];
        auto &ops = inst->getOperands();
        auto op1 = ops[1], op2 = ops[2];
        auto inst1 = ops[1]->getDef(), inst2 = ops[2]->getDef();
        if (!inst1->isPhi() && inst1->getParent() == *it) // 6
            return false;
        if (!inst2->isPhi() && inst2->getParent() == *it) // 6
            return false;
        for (auto &&pred : (*it)->getPred())
        {
            if (inst1->isPhi() && inst1->getParent() == *it)
            {
                op1 = ((PhiInstruction *)inst1)->getEdge(pred);
            }
            if (inst2->isPhi() && inst2->getParent() == *it)
            {
                op2 = ((PhiInstruction *)inst2)->getEdge(pred);
            }
            auto new_ques = new BinaryInstruction(inst->getOpCode(), ops[0], op1, op2);
            if (LCTs[pred].count(new_ques))
            { // 5
                continue;
            }
            if (questions.find(pred) != questions.end())
            {
                if (!isSameRHS(questions[pred], new_ques))
                { // 3
                    return false;
                }
                else
                { // 4
                    delete new_ques;
                }
            }
            else
            {
                questions[pred] = new_ques;
            }
        }
    }
    return true;
}

bool GlobalValueNumbering::qpGlobalSearch(std::vector<Instruction *> &may_list, Instruction *inst)
{
    auto block = inst->getParent();
    if (loopheaders.count(block))
        return false;
    auto it = find(rtop.begin(), rtop.end(), block);
    map<BasicBlock *, Instruction *> questions;
    questions[block] = inst;
    for (; it != rtop.end() - 1; it++) // 2
    {
        if (questions.find(*it) == questions.end())
            continue;
        auto inst = questions[*it];
        auto &ops = inst->getOperands();
        auto op1 = ops[1], op2 = ops[2];
        auto inst1 = ops[1]->getDef(), inst2 = ops[2]->getDef();
        if (!inst1->isPhi() && inst1->getParent() == *it) // 6
            return false;
        if (!inst2->isPhi() && inst2->getParent() == *it) // 6
            return false;
        for (auto &&pred : (*it)->getPred())
        {
            if (inst1->isPhi() && inst1->getParent() == *it)
            {
                op1 = ((PhiInstruction *)inst1)->getEdge(pred);
            }
            if (inst2->isPhi() && inst2->getParent() == *it)
            {
                op2 = ((PhiInstruction *)inst2)->getEdge(pred);
            }
            auto new_ques = new BinaryInstruction(inst->getOpCode(), ops[0], op1, op2);
            if (LCTs[pred].count(new_ques))
            {                                                // 5
                may_list.push_back(LCTs[pred][new_ques][0]); //???
                delete new_ques;
                continue;
            }
            if (questions.find(pred) != questions.end())
            {
                if (!isSameRHS(questions[pred], new_ques))
                { // 3
                    return false;
                }
                else
                { // 4
                    delete new_ques;
                }
            }
            else
            {
                questions[pred] = new_ques;
            }
        }
    }
    return true;
}

void GlobalValueNumbering::moveComputationsOutOfALoop(BasicBlock *header)
{
    assert(header->getNumOfPred() == 1);
    auto pad = *(header->pred_begin());
    for (auto inst = header->begin(); inst < header->end(); inst = inst->getNext())
    {
        if (!qpLocalSearch(inst))
            continue;
        // inst: vh <= E
        auto vp = new Operand(new TemporarySymbolEntry(inst->getDef()->getType(), SymbolTable::getLabel()));
        // copy the computation to the landing pad
        auto &ops = inst->getOperands();
        auto op1 = ops[1], op2 = ops[2];
        auto inst1 = ops[1]->getDef(), inst2 = ops[2]->getDef();
        if (inst1->isPhi() && inst1->getParent() == header)
        {
            op1 = ((PhiInstruction *)inst1)->getEdge(pad);
        }
        if (inst2->isPhi() && inst2->getParent() == header)
        {
            op2 = ((PhiInstruction *)inst2)->getEdge(pad);
        }
        MCTs[{pad, header}].push_back(
            new BinaryInstruction(inst->getOpCode(), vp, op1, op2));
        MCTps[{pad, header}].push_back(inst);
    }
    // and then eliminate the old copy when various other redundancies are eliminated
}

void GlobalValueNumbering::moveComputationsIntoPad(BasicBlock *pad)
{
    moveComputationsFromSuccessors(pad);
    // Conditions:
    // 1. For each virtual outedge, moveComputationsFromSuccessors.
    // 2. Each of the operands of the expressions must be available in the landing pad. by checking the topsort number
    vector<Instruction *> check_list, move_list;
    // TODO：virtual
    auto &checking = virtualEdge[pad];
    auto pivot = checking.begin();
    auto &m1 = MCTs[{pad, *pivot}];
    auto &o1 = MCTps[{pad, *pivot}];
    if (checking.size() == 1)
    {
        check_list.swap(m1);
    }
    else
    {
        //???
        unordered_set<Instruction *> removal_list;
        for (auto &&inst : m1)
        {
            vector<std::vector<Instruction *>::iterator>
                erases;
            auto vBB = pivot;
            for (vBB++; vBB != checking.end(); vBB++)
            {
                auto &m2 = MCTs[{pad, *vBB}];
                bool has = false;
                for (auto it = m2.begin(); it != m2.end(); it++)
                {
                    if (isSameRHS(inst, *it))
                    {
                        has = true;
                        erases.push_back(it);
                        break;
                    }
                }
                if (!has)
                {
                    erases.clear();
                    break;
                }
            }
            if (!erases.empty())
            {
                check_list.push_back(inst);
                assert(erases.size() == checking.size() - 1);
                auto vBB = pivot;
                auto idx = 0;
                for (vBB++; vBB != checking.end(); vBB++, idx++)
                {
                    auto &o2 = MCTps[{pad, *vBB}];
                    auto &m2 = MCTs[{pad, *vBB}];
                    auto &it = erases[idx];
                    auto &original = o2[it - m2.begin()];
                    if (ranks[original->getDef()] < cur_rank)
                        ranks[original->getDef()] = cur_rank;

                    removal_list.insert(inst);
                    m2.erase(it);
                    break;
                }
            }
        }
        for (auto it = m1.begin(); it != m1.end();)
        {
            if (removal_list.count(*it))
            {
                auto &original = o1[it - m1.begin()];
                if (ranks[original->getDef()] < cur_rank)
                    ranks[original->getDef()] = cur_rank;
                delete *it;
                it = m1.erase(it);
            }
            else
            {
                it++;
            }
        }
    }

    auto idx = find(rtop.begin(), rtop.end(), pad) - rtop.begin();
    for (auto &&inst : check_list)
    {
        auto &ops = inst->getOperands();
        auto inst1 = ops[1]->getDef(), inst2 = ops[2]->getDef();
        auto bb1 = inst1->getParent(), bb2 = inst2->getParent();
        auto idx1 = find(rtop.begin(), rtop.end(), bb1) - rtop.begin();
        auto idx2 = find(rtop.begin(), rtop.end(), bb2) - rtop.begin();
        if (idx1 > idx && idx2 > idx)
        {
            move_list.push_back(inst);
        }
    }
    auto j_inst = pad->rbegin();
    for (auto &&inst : move_list)
    {
        pad->insertBefore(inst, j_inst);
        LCTs[pad][inst].push_back(inst);
    }
}

void GlobalValueNumbering::eliminateGlobalRedundancies(std::vector<Instruction *> &del_list, Instruction *inst)
{
    vector<Instruction *> may_list;
    if (!qpGlobalSearch(may_list, inst))
        return;
    // Create a new variable V to remember the value that Q will (redundantly) compute.
    auto variable = new Operand(new TemporarySymbolEntry(inst->getDef()->getType(), SymbolTable::getLabel()));
    auto zero = new Operand(new ConstantSymbolEntry(inst->getDef()->getType(), 0));
    // For each of the computations C that was put on the
    // list of computations that might make Q redundant, an
    // assignment of the form V <- (output of C) is inserted.
    if (may_list.size() == 1)
    {
        auto trivial = new BinaryInstruction(
            BinaryInstruction::ADD,
            variable,
            may_list[0]->getDef(),
            zero);
        inst->getParent()->insertBefore(trivial, inst);
        trivial_worklist.push_back(trivial);
    }
    else
    {
        auto phi = new PhiInstruction(variable);
        for (auto &&c : may_list)
        {
            assert(c->getParent() != nullptr);
            phi->addEdge(c->getParent(), c->getDef());
        }
        auto it = inst->getParent()->begin();
        for (; it != inst->getParent()->end(); it = it->getNext())
        {
            if (!it->isPhi())
                break;
        }
        inst->getParent()->insertBefore(phi, it);
    }

    // The expression part of Q is replaced with a use of V.
    auto trivial = new BinaryInstruction(
        BinaryInstruction::ADD,
        inst->getDef(),
        variable,
        zero);
    inst->getParent()->insertBefore(trivial, inst);
    trivial_worklist.push_back(trivial);
    del_list.push_back(inst);
}
