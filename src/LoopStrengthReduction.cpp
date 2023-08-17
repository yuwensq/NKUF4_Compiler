#include "LoopCodeMotion.h"

using namespace std;

void LoopCodeMotion::pass2()
{
    if (!zero)
        zero = new Operand(new ConstantSymbolEntry(TypeSystem::intType, 0));
    if (!one)
        one = new Operand(new ConstantSymbolEntry(TypeSystem::intType, 1));
    // 遍历每一个函数做操作
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        clearData();
        // 计算当前函数每一个基本块的必经节点，存入DomBBSet
        calculateFinalDomBBSet(*func);
        // printDomBB(*func);

        // 计算当前函数的回边集合
        std::vector<std::pair<BasicBlock *, BasicBlock *>> BackEdges = getBackEdges(*func);
        // printBackEdges(BackEdges);

        // 获取回边组，要求每个组中的回边的到达节点是同一个
        std::vector<std::vector<std::pair<BasicBlock *, BasicBlock *>>> edgeGroups = mergeEdge(BackEdges);
        // printEdgeGroups(edgeGroups);

        // 查找当前函数的循环体的集合
        std::vector<std::vector<BasicBlock *>> LoopList = calculateLoopList(*func, edgeGroups);
        // for (auto &Loop : LoopList)
        // {
        //     std::cerr << "Loop size:" << Loop.size() << std::endl;
        //     for (auto &block : Loop)
        //         std::cerr << block->getNo() << " ";
        //     std::cerr << std::endl;
        // }

        // auto &domBBSet = getDomBBSet(*func);

        // 强度削弱
        for (auto &Loop : LoopList)
        {
            auto preheader = new BasicBlock(*func);
            preprocess(preheader, Loop, BackEdges);
        }

        clearData();
        calculateFinalDomBBSet(*func);
        getBackEdges(*func).swap(BackEdges);
        mergeEdge(BackEdges).swap(edgeGroups);
        calculateLoopList(*func, edgeGroups).swap(LoopList);

        for (auto &Loop : LoopList)
        {
            assert(Loop[0]->getPredRef().size() == 2);
            for (auto &preheader : Loop[0]->getPredRef())
            {
                if (find(BackEdges.begin(), BackEdges.end(), make_pair(preheader, Loop[0])) != BackEdges.end())
                    continue;
                LoopStrengthReduction(preheader, Loop);
                break;
            }
        }
        elimSelfIVs(*func);
    }
}

void LoopCodeMotion::preprocess(
    BasicBlock *preheader,
    std::vector<BasicBlock *> &Loop,
    std::vector<std::pair<BasicBlock *, BasicBlock *>> &BackEdges)
{
    BasicBlock *headBlock = Loop[0];
    std::vector<BasicBlock *> pre_block_delete;

    // 查headblock前驱，将其最后一条指令然后改掉，指向predBB
    // 遍历该循环的首节点的所有前继节点
    for (auto block = headBlock->pred_begin(); block != headBlock->pred_end(); block++)
    {
        std::pair<BasicBlock *, BasicBlock *> edge(*block, headBlock);
        if (find(BackEdges.begin(), BackEdges.end(), edge) != BackEdges.end())
        {
            continue;
        }

        pre_block_delete.push_back(*block);
        // 获取该前继基本块的最后一条指令
        Instruction *lastins = (*block)->rbegin();
        // 如果是条件跳转
        if (lastins->isCond())
        {
            CondBrInstruction *last = (CondBrInstruction *)lastins;
            // headBlock有可能是从真分支跳来的or假分支跳来的
            if (last->getTrueBranch() == headBlock)
            {
                last->setTrueBranch(preheader);
            }
            else if (last->getFalseBranch() == headBlock)
            {
                last->setFalseBranch(preheader);
            }
        }
        // 如果是非条件跳转
        else if (lastins->isUncond())
        {
            UncondBrInstruction *last = (UncondBrInstruction *)lastins;
            last->setBranch(preheader);
        }
        (*block)->removeSucc(headBlock);
        (*block)->addSucc(preheader);
        preheader->addPred(*block);
    }

    headBlock->addPred(preheader);
    preheader->addSucc(headBlock);
    new UncondBrInstruction(headBlock, preheader);
    for (auto &block : pre_block_delete)
    {
        // std::cout<<block->getNo()<<std::endl;
        headBlock->removePred(block);
    }
    // phi指令,这个处理
    changePhiInstruction(Loop, preheader, pre_block_delete);

    // 增加latch
    auto &loopheader = headBlock;
    if (loopheader->getNumOfPred() == 2)
        return;
    auto latch = new BasicBlock(loopheader->getParent());
    Log("latch: %d", latch->getNo());
    Loop.push_back(latch);
    // TODO：add a latch
    auto &preds = loopheader->getPredRef();
    for (auto it = preds.begin(); it != preds.end();)
    {
        if (*it == preheader)
        {
            it++;
            continue;
        }
        auto &pred = *it;
        latch->addPred(pred);
        pred->addSucc(latch);
        if (pred->rbegin()->isCond())
        {
            auto j_inst = (CondBrInstruction *)pred->rbegin();
            if (j_inst->getFalseBranch() == loopheader)
            {
                j_inst->setFalseBranch(latch);
            }
            else if (j_inst->getTrueBranch() == loopheader)
            {
                j_inst->setTrueBranch(latch);
            }
            else
                assert(0);
        }
        else if (pred->rbegin()->isUncond())
        {
            ((UncondBrInstruction *)pred->rbegin())->setBranch(latch);
        }
        else
            assert(0);
        pred->removeSucc(loopheader);
        it = preds.erase(it);
    }
    for (auto inst = loopheader->begin(); inst != loopheader->end(); inst = inst->getNext())
    {
        if (!inst->isPhi())
            break;
        auto origin = (PhiInstruction *)inst;
        auto dest = new PhiInstruction(new Operand(new TemporarySymbolEntry(inst->getDef()->getType(), SymbolTable::getLabel())), latch);
        auto &srcs = origin->getSrcs();
        for (auto it = srcs.begin(); it != srcs.end();)
        {
            auto &[pred, v] = *it;
            if (pred == preheader)
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
        origin->addEdge(latch, dest->getDef());
        dest->getDef()->setDef(dest);
    }
    loopheader->addPred(latch);
    latch->addSucc(loopheader);
    new UncondBrInstruction(loopheader, latch);
}

void LoopCodeMotion::LoopStrengthReduction(BasicBlock *preheader, std::vector<BasicBlock *> &Loop)
{
    assert(Loop[0]->getNumOfPred() == 2);
    auto headpreds = Loop[0]->getPred();
    auto latch = headpreds[0] == preheader ? headpreds[1] : headpreds[0];

    IVs.clear();
    std::unordered_map<Operand *, int> BIVs;
    for (auto inst = Loop[0]->begin(); inst != Loop[0]->end(); inst = inst->getNext())
    {
        if (!inst->isPhi())
            break;
        auto phi = (PhiInstruction *)inst;
        auto def = phi->getDef();
        auto src = phi->getBlockSrc(latch);
        auto srcdef = src->getDef();
        assert(srcdef);
        if (!srcdef->isBinary())
            continue;
        auto &v = srcdef->getOperands();
        if (srcdef->getOpCode() == BinaryInstruction::SUB || srcdef->getOpCode() == BinaryInstruction::ADD)
        {
            if (v[1] == def)
            {
                int n = ((ConstantSymbolEntry *)v[2]->getEntry())->getValue();
                BIVs[def] = srcdef->getOpCode() == BinaryInstruction::SUB ? -n : n;
                IVs[def] = {def, 1, 0};
            }
            else if (v[2] == def)
            {
                if (srcdef->getOpCode() == BinaryInstruction::ADD)
                {
                    int n = ((ConstantSymbolEntry *)v[1]->getEntry())->getValue();
                    BIVs[def] = n;
                    IVs[def] = {def, 1, 0};
                }
            }
        }
    }
    findNonbasicInductionVariables(Loop);
    for (auto &[iv, cc] : IVs)
    {
        auto &[a, b, c] = cc;
        Log("%s: %s,%d,%d", iv->toStr().c_str(), a->toStr().c_str(), b, c);
    }
    Log("");

    unordered_map<Operand *, Operand *> phiMap;
    auto j_inst = preheader->rbegin();
    vector<Instruction *> ins_list;
    for (auto inst = Loop[0]->begin(); inst != Loop[0]->end(); inst = inst->getNext())
    {
        // we insert after the phi node
        if (!inst->isPhi())
        {
            for (auto &phi : ins_list)
            {
                Loop[0]->insertBefore(phi, inst);
            }
            break;
        }
        auto phi = (PhiInstruction *)inst;
        auto value = phi->getBlockSrc(preheader);
        for (auto &[iv, cc] : IVs)
        {
            if (cc.basic_induction_variable == phi->getDef() && (cc.multiplicative != 1 || cc.additive != 0)) // not a basic
            {
                auto *_1 = new Operand(new TemporarySymbolEntry(value->getType(), SymbolTable::getLabel())),
                     *_2 = new Operand(new TemporarySymbolEntry(value->getType(), SymbolTable::getLabel())),
                     *_3 = new Operand(new TemporarySymbolEntry(value->getType(), SymbolTable::getLabel()));
                // Initialize a non-basic induction variable with the computed initial value in the loop preheader;
                preheader->insertBefore(
                    new BinaryInstruction(
                        BinaryInstruction::MUL, _1,
                        value, new Operand(new ConstantSymbolEntry(TypeSystem::intType, cc.multiplicative))),
                    j_inst);
                preheader->insertBefore(
                    new BinaryInstruction(
                        BinaryInstruction::ADD, _2,
                        _1, new Operand(new ConstantSymbolEntry(TypeSystem::intType, cc.additive))),
                    j_inst);
                auto phinode = new PhiInstruction(_3);
                _3->setDef(phinode);
                phinode->addSrc(preheader, _2);
                phiMap[iv] = _3;
                // Add one Phi node for this non-basic induction variable into the loop header, and set the incoming value of the Phi node to the initial value we just computed;
                ins_list.push_back(phinode);
            }
        }
    }

    // modify the new latch block by inserting cheaper computation
    j_inst = latch->rbegin();
    ins_list.clear();
    for (auto &block : Loop)
        for (auto inst = block->begin(); inst != block->end(); inst = inst->getNext())
        {
            if (!inst->isBinary())
                continue;
            auto iv = inst->getDef();
            if (phiMap.find(iv) == phiMap.end())
                continue;
            // TODO：相同的只生成一个，现在先交给公共子表达式删除吧
            auto cc = IVs[iv];
            // auto insert_pos = ((PhiInstruction *)cc.basic_induction_variable->getDef())->getBlockSrc(latch)->getDef();
            // auto *lhs = insert_pos->getOperands()[1], *rhs = insert_pos->getOperands()[2];
            // int n = ((ConstantSymbolEntry *)(lhs == cc.basic_induction_variable ? rhs : lhs)->getEntry())->getValue();
            int n = BIVs[cc.basic_induction_variable];
            int incre = cc.multiplicative * n;
            auto phidef = phiMap[iv];
            auto t = new Operand(new TemporarySymbolEntry(phidef->getType(), SymbolTable::getLabel()));
            ins_list.push_back(
                new BinaryInstruction(
                    BinaryInstruction::ADD, t,
                    phidef, new Operand(new ConstantSymbolEntry(TypeSystem::intType, incre))));
            ((PhiInstruction *)phidef->getDef())->addSrc(latch, t);
            inst = inst->getPrev();
            block->strongRemove(inst->getNext());
        }
    for (auto &ins : ins_list)
    {
        latch->insertBefore(ins, j_inst);
    }

    // Replace all uses of the original induction variable with the Phi node we inserted.
    // replace all the original uses with phi-node
    for (auto &[old, rep] : phiMap)
    {
        // for (auto &use : old->getUse())
        while (!old->getUse().empty())
        {
            auto use = *old->getUse().begin();
            use->replaceUse(old, rep);
        }
        assert(old->getUse().size() == 0);
        // Go through the uses list for this definition and make each use point to "V" instead of "this".
        // After this completes, 'this's use list is guaranteed to be empty.
    }
}

void LoopCodeMotion::findNonbasicInductionVariables(std::vector<BasicBlock *> &Loop)
{
    while (true)
    {
        auto newIVs = IVs;
        for (auto &block : Loop)
        {
            for (auto inst = block->begin(); inst != block->end(); inst = inst->getNext())
            {
                if (inst->isBinary())
                {
                    Operand *lhs = inst->getOperands()[1], *rhs = inst->getOperands()[2];
                    if (IVs.count(lhs) || IVs.count(rhs))
                    {
                        // bool cil = lcOps[lhs], cir = lcOps[rhs];
                        bool cil = lhs->getEntry()->isConstant(), cir = rhs->getEntry()->isConstant();
                        auto itl = IVs.find(lhs), itr = IVs.find(rhs);
                        switch (inst->getOpCode())
                        {
                        case BinaryInstruction::ADD:
                            if (itl != IVs.end() && cir)
                            {
                                auto &t = itl->second;
                                newIVs[inst->getDef()] = {
                                    t.basic_induction_variable,
                                    t.multiplicative,
                                    t.additive + (int)((ConstantSymbolEntry *)rhs->getEntry())->getValue()};
                            }
                            else if (itr != IVs.end() && cil)
                            {
                                auto &t = itr->second;
                                newIVs[inst->getDef()] = {
                                    t.basic_induction_variable,
                                    t.multiplicative,
                                    t.additive + (int)((ConstantSymbolEntry *)lhs->getEntry())->getValue()};
                            }
                            break;
                        case BinaryInstruction::SUB:
                            if (itl != IVs.end() && cir)
                            {
                                auto &t = itl->second;
                                newIVs[inst->getDef()] = {
                                    t.basic_induction_variable,
                                    t.multiplicative,
                                    t.additive - (int)((ConstantSymbolEntry *)rhs->getEntry())->getValue()};
                            }
                            else if (itr != IVs.end() && cil)
                            {
                                auto &t = itr->second;
                                newIVs[inst->getDef()] = {
                                    t.basic_induction_variable,
                                    -t.multiplicative,
                                    (int)((ConstantSymbolEntry *)lhs->getEntry())->getValue() - t.additive};
                            }
                            break;
                        case BinaryInstruction::MUL:
                            if (itl != IVs.end() && cir)
                            {
                                auto &t = itl->second;
                                newIVs[inst->getDef()] = {
                                    t.basic_induction_variable,
                                    t.multiplicative * (int)((ConstantSymbolEntry *)rhs->getEntry())->getValue(),
                                    t.additive * (int)((ConstantSymbolEntry *)rhs->getEntry())->getValue()};
                            }
                            else if (itr != IVs.end() && cil)
                            {
                                auto &t = itr->second;
                                newIVs[inst->getDef()] = {
                                    t.basic_induction_variable,
                                    t.multiplicative * (int)((ConstantSymbolEntry *)lhs->getEntry())->getValue(),
                                    t.additive * (int)((ConstantSymbolEntry *)lhs->getEntry())->getValue()};
                            }
                            break;

                        default:
                            break;
                        }
                    }
                }
            }
        }
        if (IVs.size() == newIVs.size())
            break;
        else
            IVs = newIVs;
    }
}

void LoopCodeMotion::elimSelfIVs(Function *func)
{
    for (auto &block : func->getBlockList())
    {
        for (auto inst = block->begin(); inst != block->end(); inst = inst->getNext())
        {
            if (!inst->isPhi())
                break;
            auto def = inst->getDef();
            if (def->getUse().size() == 0)
            {
                inst = inst->getPrev();
                block->strongRemove(inst->getNext());
            }
            else if (def->getUse().size() == 1)
            {
                auto user = def->getUse()[0];
                if (!user->getDef())
                    continue;
                auto &v = user->getDef()->getUse();
                if (v.size() == 1 && v[0] == inst)
                {
                    user->getParent()->strongRemove(user);
                    inst = inst->getPrev();
                    block->strongRemove(inst->getNext());
                    if (IVs.count(def))
                    {
                        auto phi = (PhiInstruction*)inst;
                        auto &srcs = phi->getSrcs();
                        for (auto &[bb, op] : srcs)
                        {
                            if (bb == user->getParent())
                            continue;
                            assert(op->getDef()->isAdd());
                            assert(op->getUse().size() == 0);
                            auto _add = op->getDef();
                            assert(_add->getPrev()->getOpCode() == BinaryInstruction::MUL);
                            _add->getParent()->strongRemove(_add->getPrev());
                            _add->getParent()->strongRemove(_add);
                        }
                    }
                }
                else
                    continue;
            }
            else
                continue;
        }
    }
}
