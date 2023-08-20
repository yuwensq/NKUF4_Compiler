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
        eliminateRedundancies(*it);
        removeTrivialAssignments();
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
    bool c[] = {a1->getType()->isConst(),
                b1->getType()->isConst(),
                a2->getType()->isConst(),
                b2->getType()->isConst()};
    if (a1 == b1 || c[0] && c[1] && (((ConstantSymbolEntry *)a1->getEntry())->getValue() == ((ConstantSymbolEntry *)b1->getEntry())->getValue()))
    {
        return a2 == b2 || c[2] && c[3] && (((ConstantSymbolEntry *)a2->getEntry())->getValue() == ((ConstantSymbolEntry *)b2->getEntry())->getValue());
    }
    if (a->getOpCode() == BinaryInstruction::SUB || a->getOpCode() == BinaryInstruction::DIV || a->getOpCode() == BinaryInstruction::MOD)
    {
        return false;
    }
    if (a1 == b2 || c[0] && c[3] && (((ConstantSymbolEntry *)a1->getEntry())->getValue() == ((ConstantSymbolEntry *)b2->getEntry())->getValue()))
    {
        return a2 == b1 || c[2] && c[1] && (((ConstantSymbolEntry *)a2->getEntry())->getValue() == ((ConstantSymbolEntry *)b1->getEntry())->getValue());
    }
    return false;
}

void GlobalValueNumbering::preprocess(Function *func)
{
    rtop.clear();
    loopheaders.clear();
    landingpads.clear();
    ranks.clear();
    LCTs.clear();
    MCTs.clear();
    MCTps.clear();
    unordered_set<BasicBlock *> visited;
    func->genReversedTopsort_N_LoopHeader(rtop, loopheaders);
    int iewno = 0;
    for (auto it = rtop.rbegin(); it != rtop.rend(); visited.insert(*it++))
    {
        auto &BB = *it;
        if (loopheaders.count(BB))
        {
            // ??? judge pre-test block by topsort order
            auto &loopheader = BB;
            int num = 0;
            // 2. add a landing pad
            auto landingpad = new BasicBlock(func);
            landingpads.insert(landingpad);
            // 1. find entrance
            for (auto test = loopheader->pred_begin(); test != loopheader->pred_end();)
            {
                auto &pred = *test;
                if (visited.find(pred) == visited.end())
                {
                    test++;
                    continue;
                }
                else
                {
                    landingpad->addPred(pred);
                    pred->addSucc(landingpad);
                    assert(pred->rbegin()->isCond());
                    auto j_inst = (CondBrInstruction *)pred->rbegin();
                    if (j_inst->getFalseBranch() == loopheader)
                    {
                        j_inst->setFalseBranch(landingpad);
                        // auto next = j_inst->getTrueBranch();
                    }
                    else if (j_inst->getTrueBranch() == loopheader)
                    {
                        j_inst->setTrueBranch(landingpad);
                        // auto next = j_inst->getFalseBranch();
                    }
                    else
                        assert(0);

                    num++;
                    pred->removeSucc(loopheader);
                    test = loopheader->getPredRef().erase(test);
                }
            }
            Log("loopheader:%d", iewno = loopheader->getNo());
            assert(num == loopheader->getNumOfPred());
            if (num == 1)
            {
                for (auto inst = loopheader->begin(); inst != loopheader->end(); inst = inst->getNext())
                {
                    if (!inst->isPhi())
                        break;
                    auto origin = (PhiInstruction *)inst;
                    auto &srcs = origin->getSrcs();
                    auto it = srcs.begin();
                    if (visited.find(it->first) == visited.end())
                    {
                        it++;
                    }
                    auto v = it->second;
                    it = srcs.erase(it);
                    origin->removeUse(v);
                    v->removeUse(origin);
                    origin->addEdge(landingpad, v);
                }
            }
            else
            {
                for (auto inst = loopheader->begin(); inst != loopheader->end(); inst = inst->getNext())
                {
                    if (!inst->isPhi())
                        break;
                    auto origin = (PhiInstruction *)inst;
                    auto dest = new PhiInstruction(new Operand(new TemporarySymbolEntry(inst->getDef()->getType(), SymbolTable::getLabel())), landingpad);
                    auto &srcs = origin->getSrcs();
                    for (auto it = srcs.begin(); it != srcs.end();)
                    {
                        auto &[pred, v] = *it;
                        if (visited.find(pred) == visited.end())
                        {
                            it++;
                            continue;
                        }
                        else
                        {
                            dest->addEdge(pred, v);
                            origin->removeUse(v);
                            v->removeUse(origin);
                            it = srcs.erase(it);
                        }
                    }
                    Log("size=%ld", srcs.size());
                    assert(srcs.size() == (size_t)loopheader->getNumOfPred());
                    origin->addEdge(landingpad, dest->getDef());
                    dest->getDef()->setDef(dest);
                }
            }
            // 3. find exit
            // 4. add a virtual edge
            // virtualEdge[landingpad].insert(next);
            // virtualREdge[next].insert(landingpad);
            assert(num == loopheader->getNumOfPred());
            loopheader->addPred(landingpad);
            landingpad->addSucc(loopheader);
            new UncondBrInstruction(loopheader, landingpad);
            for (auto inst = landingpad->begin(); inst != landingpad->end(); inst = inst->getNext())
            {
                assignRanks(inst);
                if (inst->isAdd() && inst->getOperands()[2]->toStr() == "0")
                {
                    if (inst->getOperands()[1]->getEntry()->isTemporary() && !((TemporarySymbolEntry *)inst->getOperands()[1]->getEntry())->isParam())
                    {
                        trivial_worklist.push_back(inst);
                    }
                }
                else if (inst->isBinary())
                {
                    LCTs[landingpad][inst].push_back(inst);
                }
            }
        }
        for (auto inst = BB->begin(); inst != BB->end(); inst = inst->getNext())
        {
            assignRanks(inst);
            if (inst->isAdd() && inst->getOperands()[2]->toStr() == "0")
            {
                if (inst->getOperands()[1]->getEntry()->isTemporary() && !((TemporarySymbolEntry *)inst->getOperands()[1]->getEntry())->isParam())
                {
                    trivial_worklist.push_back(inst);
                }
            }
            else if (inst->isBinary())
            {
                LCTs[BB][inst].push_back(inst);
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
    Log("remove start,%d", trivial_worklist.size());
    for (auto &&inst : trivial_worklist)
    {
        auto &ops = inst->getOperands();
        auto &A = ops[0], &B = ops[1];
        Log("inst: %s", inst->getDef()->toStr().c_str());
        if (B->getUse().size() > A->getUse().size())
        {
            for (auto &&use_inst : A->getUse())
            {
                if (use_inst->isBinary() && LCTs[use_inst->getParent()].count(use_inst))
                {
                    auto &tmp = LCTs[use_inst->getParent()][use_inst];
                    tmp.erase(find(tmp.begin(), tmp.end(), use_inst));
                    use_inst->replaceUse(A, B);
                    LCTs[use_inst->getParent()][use_inst].push_back(use_inst);
                }
                else
                {
                    use_inst->replaceUse(A, B);
                }
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
            Log(">");
            Log("A:%s", A->toStr().c_str());
            Log("B:%s", B->toStr().c_str());
            /*
            C <= E
            B <= C + 0
            A <= B + 0
            */
            for (auto &&use_inst : B->getUse())
            {
                if (use_inst == inst)
                    continue;
                if (use_inst->isBinary() && LCTs[use_inst->getParent()].count(use_inst))
                {
                    auto &tmp = LCTs[use_inst->getParent()][use_inst];
                    tmp.erase(find(tmp.begin(), tmp.end(), use_inst));
                    use_inst->replaceUse(B, A);
                    LCTs[use_inst->getParent()][use_inst].push_back(use_inst);
                }
                else
                {
                    use_inst->replaceUse(B, A);
                }
            }
            Log("remove B-0");
            auto tmp = B->getDef();
            auto AA = A;
            inst->setDef(B);
            tmp->replaceDef(AA);
            Log("remove B-2");
        }
        Log("A:%s", A->toStr().c_str());
        Log("B:%s", B->toStr().c_str());
        inst->getParent()->remove(inst);
        // delete inst;
    }
    trivial_worklist.clear();
    Log("eliminate start");
    return eliminateLocalRedundancies();
}

void GlobalValueNumbering::eliminateLocalRedundancies()
{
    for (auto &&[block, list] : LCTs)
    {
        Log("BB:%d", block->getNo());
        for (auto inst = block->begin(); inst != block->end(); inst = inst->getNext())
        {
            if (inst->isBinary())
            {
                if (list.count(inst) && list[inst].size() > 1)
                {
                    for (auto it = list[inst].begin(); it != list[inst].end();)
                    {
                        auto &elim = *it;
                        if (elim == inst)
                        {
                            it++;
                            continue;
                        }
                        auto trivial = new BinaryInstruction(
                            BinaryInstruction::ADD,
                            elim->getDef(),
                            inst->getDef(),
                            new Operand(new ConstantSymbolEntry(elim->getDef()->getType(), 0)));
                        block->insertBefore(trivial, elim);
                        trivial_worklist.push_back(trivial);
                        block->remove(elim);
                        // delete elim;
                        it = list[inst].erase(it);
                    }
                    vector<Instruction *>({inst}).swap(list[inst]);
                    Log("inst: %s", inst->getDef()->toStr().c_str());
                }
            }
        }
    }
    Log("end");
    if (!trivial_worklist.empty())
    {
        Log("end2");
        return removeTrivialAssignments();
    }
}

void GlobalValueNumbering::eliminateRedundancies(Function *func)
{
    if (ranks.empty())
        return;
    max_rank = max_element(ranks.begin(), ranks.end(),
                           [](const pair<Operand *, int> &a, const pair<Operand *, int> &b)
                           { return a.second < b.second; })
                   ->second;
    Log("max_rank=%d", max_rank);
    for (cur_rank = 0; cur_rank < max_rank; cur_rank++)
    {
        // for each node n (in reverse topsort order) do
        for (auto &BB : rtop)
        {
            Log("BB:%d", BB->getNo());
            if (landingpads.count(BB))
            {
                Log("landingpads");
                moveComputationsFromSuccessors(BB);
                moveComputationsIntoPad(BB);
                identifyMovableComputations(BB);
            }
            else if (loopheaders.count(BB))
            {
                Log("loopheaders");
                moveComputationsFromSuccessors(BB);
                identifyMovableComputations2(BB);
            }
            else
            {
                Log("normal");
                moveComputationsFromSuccessors(BB);
                identifyMovableComputations(BB);
            }
        }

        Log("Elimination start.");
        for (auto it = rtop.rbegin(); it != rtop.rend(); it++)
        {
            vector<Instruction *> del_list;
            for (auto inst = (*it)->begin(); inst != (*it)->end(); inst = inst->getNext())
            {
                if (!inst->isBinary())
                    continue;
                if (ranks[inst->getDef()] != cur_rank)
                    continue;
                Log("inst:%s", inst->getDef()->toStr().c_str());
                auto &ops = inst->getOperands();
                if (ranks[ops[0]] > max(ranks[ops[2]], ranks[ops[1]]) + 1)
                {
                    for (auto &use_inst : ops[0]->getUse())
                    {
                        if (use_inst->getParent() != inst->getParent())
                            continue;
                        if (ranks[use_inst->getDef()] < cur_rank + 1)
                            ranks[use_inst->getDef()] = cur_rank + 1;
                    }
                }
                Log("eliminateGlobalRedundancies");
                eliminateGlobalRedundancies(del_list, inst);
                Log("eliminateGlobalRedundancies");
            }
            Log("del_list");
            for (auto &&inst : del_list)
            {
                //???
                auto &tmp = LCTs[inst->getParent()][inst];
                tmp.erase(find(tmp.begin(), tmp.end(), inst));
                if (!tmp.empty())
                {
                    LCTs[inst->getParent()][tmp[0]] = tmp;
                }
                else
                {
                    LCTs[inst->getParent()].erase(inst);
                }
                inst->getParent()->remove(inst);
                // delete inst;
            }
            Log("pause");
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
                // delete *it;
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
        if (!inst->isBinary())
            continue;
        auto &ops = inst->getOperands();
        if (ranks[ops[0]] != cur_rank)
            continue;
        // TODO：有一个op是之前BB中的计算结果怎么处理？目前认为符合 movable 条件之一
        auto op1 = ops[1], op2 = ops[2];
        auto inst1 = op1->getDef(), inst2 = op2->getDef();
        if (inst1 == nullptr || inst2 == nullptr)
        {
            Log("non-def:%s", inst->getDef()->toStr().c_str());
            continue;
        }
        if (!inst1->isPhi() && inst1->getParent() == block)
            continue;
        if (!inst2->isPhi() && inst2->getParent() == block)
            continue;
        for (auto &&pred : block->getPredRef())
        {
            if (inst1->isPhi() && inst1->getParent() == block)
            {
                op1 = ((PhiInstruction *)inst1)->getEdge(pred);
            }
            if (inst2->isPhi() && inst2->getParent() == block)
            {
                op2 = ((PhiInstruction *)inst2)->getEdge(pred);
            }
            MCTs[{pred, block}].push_back(
                new BinaryInstruction(
                    inst->getOpCode(),
                    new Operand(new TemporarySymbolEntry(inst->getDef()->getType(), SymbolTable::getLabel())),
                    op1, op2));
            MCTps[{pred, block}].push_back(inst);
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
    unordered_map<BasicBlock *, Instruction *> questions;
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
        if (inst1 == nullptr || inst2 == nullptr)
        {
            Log("non-def:%s", inst->getDef()->toStr().c_str());
            continue;
        }
        if (!inst1->isPhi() && inst1->getParent() == *it) // 6
            return false;
        if (!inst2->isPhi() && inst2->getParent() == *it) // 6
            return false;
        for (auto &&pred : (*it)->getPredRef())
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

void rtopsortPreds(std::vector<BasicBlock *> &rtop, BasicBlock *block, std::unordered_set<BasicBlock *> &visited)
{
    visited.insert(block);
    for (auto &pred : block->getPredRef())
    {
        if (visited.find(pred) == visited.end())
        {
            rtopsortPreds(rtop, pred, visited);
        }
    }
    rtop.push_back(block);
}

bool GlobalValueNumbering::qpGlobalSearch(std::vector<Instruction *> &may_list, Instruction *inst)
{
    auto source = inst->getParent();
    // auto it = find(rtop.begin(), rtop.end(), source);
    unordered_map<BasicBlock *, Instruction *> questions;
    questions[source] = inst;
    Log("prog-inst: %s = %s %s", inst->getDef()->toStr().c_str(), inst->getUse()[0]->toStr().c_str(), inst->getUse()[1]->toStr().c_str());
    unordered_set<BasicBlock *> visited;
    vector<BasicBlock *> preds;
    rtopsortPreds(preds, source, visited);
    while (!preds.empty()) // 2
    {
        auto block = preds.back();
        preds.pop_back();
        if (questions.find(block) == questions.end())
            continue;
        auto inst = questions[block];
        Log("prog:%d", block->getNo());
        auto &ops = inst->getOperands();
        auto op1 = ops[1], op2 = ops[2];
        auto inst1 = ops[1]->getDef(), inst2 = ops[2]->getDef();
        if (inst1 == nullptr || inst2 == nullptr)
        {
            Log("non-def:%s", inst->getDef()->toStr().c_str());
            continue;
        }
        if (!inst1->isPhi() && inst1->getParent() == block) // 6
            return false;
        if (!inst2->isPhi() && inst2->getParent() == block) // 6
            return false;
        for (auto &&pred : block->getPredRef())
        {
            if (visited.find(pred) != visited.end())
                continue;
            if (inst1->isPhi() && inst1->getParent() == block)
            {
                op1 = ((PhiInstruction *)inst1)->getEdge(pred);
            }
            if (inst2->isPhi() && inst2->getParent() == block)
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
    // moveComputationsFromSuccessors(pad);
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
                // delete *it;
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
    Log("qpg start.");
    if (!qpGlobalSearch(may_list, inst))
        return;
    Log("qpg end.");
    if (may_list.empty())
        return;
    del_list.push_back(inst);
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
        Log("Phi.%s", variable->toStr().c_str());
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
}
