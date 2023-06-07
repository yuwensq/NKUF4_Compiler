#include "FunctionInline.h"
#include <queue>

// #define INLINEDEBUG

void FunctionInline::clearData()
{
    callIns.clear();
    exitBlocks.clear();
    inlinedFuncs.clear();
}

void FunctionInline::preProcess()
{
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        for (auto bb : (*func)->getBlockList())
        {
            for (auto inst = bb->begin(); inst != bb->end(); inst = inst->getNext())
            {
                if (!inst->isCall())
                    continue;
                auto funcSe = static_cast<CallInstruction *>(inst)->getFunc();
                if (static_cast<IdentifierSymbolEntry *>(funcSe)->isSysy())
                    continue;
                auto calledFunc = unit->se2Func(funcSe);
                callIns[calledFunc].insert(inst);
            }
        }
    }
}

bool FunctionInline::shouldBeInlined(Function *func)
{
    // 直接递归不内联
    for (auto inst : callIns[func])
        if (inst->getParent()->getParent() == func)
            return false;
    // 参数大于10内联
    if (func->getParams().size() >= 8)
        return true;
    // 指令数太多不内联
    long long inst_num = 0;
    for (auto bb : func->getBlockList())
    {
        for (auto inst = bb->begin(); inst != bb->end(); inst = inst->getNext())
        {
            if (inst->isAlloc() || inst->isCond() || inst->isUncond())
                continue;
            inst_num++;
        }
        if (inst_num >= 50)
            return false;
    }
    return true;
}

Operand *FunctionInline::copyOp(Operand *op)
{
    return new Operand(new TemporarySymbolEntry(op->getType(), SymbolTable::getLabel()));
}

void FunctionInline::copyFunc(Instruction *calledInst, Function *calleeFunc)
{
    entryBlock = nullptr;
    exitBlocks.clear();
    auto callerFunc = calledInst->getParent()->getParent();
    auto params = calledInst->getUse();
    std::map<BasicBlock *, BasicBlock *> blk2blk;
    std::map<Operand *, Operand *> op2op;
    blk2blk.clear();
    op2op.clear();
    // 先复制每个基本块
    for (auto bb : calleeFunc->getBlockList())
    {
        auto newBB = new BasicBlock(callerFunc);
        if (bb == calleeFunc->getEntry())
            entryBlock = newBB;
        blk2blk[bb] = newBB;

        for (auto inst = bb->begin(); inst != bb->end(); inst = inst->getNext())
        {
            // 如果是ret指令，要用uncond指令替换
            if (inst->isRet())
            {
                Operand *retValue = nullptr;
                if (inst->getUse().size())
                {
                    auto use = inst->getUse()[0];
                    auto se = use->getEntry();
                    if (se->isConstant())
                        retValue = new Operand(se);
                    else if (op2op.find(use) == op2op.end())
                    {
                        retValue = copyOp(use);
                        op2op[use] = retValue;
                    }
                    else
                        retValue = op2op[use];
                }
                auto uncondIns = new UncondBrInstruction(nullptr, newBB);
                exitBlocks[newBB] = std::make_pair(uncondIns, retValue);
                break;
            }
            auto newInst = inst->copy();
            Assert(newInst != nullptr, "这里为啥嘞");
            newInst->setParent(newBB);
            newBB->insertBack(newInst);
            auto def = newInst->getDef();
            if (def != nullptr)
            {
                if (op2op.find(def) == op2op.end())
                    op2op[def] = copyOp(def);
                newInst->replaceDef(op2op[def]);
            }
            auto uses = newInst->getUse();
            for (auto use : uses)
            {
                int paramNo = -1;
                auto newUse = use;
                auto se = use->getEntry();
                if (se->isVariable() && static_cast<IdentifierSymbolEntry *>(se)->isGlobal())
                    newUse = new Operand(se);
                else if (se->isConstant())
                    newUse = new Operand(se);
                else if ((paramNo = calleeFunc->getParamNumber(use)) != -1)
                    newUse = params[paramNo];
                else
                {
                    if (op2op.find(use) == op2op.end())
                        op2op[use] = copyOp(use);
                    newUse = op2op[use];
                }
                newInst->replaceUse(use, newUse);
            }
            if (newInst->isCall())
            {
                auto funcSe = static_cast<CallInstruction *>(newInst)->getFunc();
                auto func = unit->se2Func(funcSe);
                callIns[func].insert(newInst);
            }
        }
    }
    // 再把每个基本块连起来
    for (auto pa : blk2blk)
    {
        auto oldBlk = pa.first;
        auto newBlk = pa.second;
        // for (auto pred = oldBlk->pred_begin(); pred != oldBlk->pred_end(); pred++)
        // {
        //     newBlk->addPred(blk2blk[*pred]);
        //     blk2blk[*pred]->addSucc(newBlk);
        // }
        for (auto succ = oldBlk->succ_begin(); succ != oldBlk->succ_end(); succ++)
        {
            newBlk->addSucc(blk2blk[*succ]);
            blk2blk[*succ]->addPred(newBlk);
        }
        // 处理phi和cond、uncond
        for (auto inst = newBlk->begin(); inst != newBlk->end(); inst = inst->getNext())
        {
            if (!inst->isPhi())
                break;
            auto &srcs = static_cast<PhiInstruction *>(inst)->getSrcs();
            auto tmp_srcs = srcs;
            for (auto src : tmp_srcs)
            {
                auto op = src.second;
                srcs.erase(src.first);
                srcs.insert(std::make_pair(blk2blk[src.first], op));
            }
        }
        for (auto inst = newBlk->rbegin(); inst != newBlk->end(); inst = inst->getPrev())
        {
            if (!(inst->isCond() || inst->isUncond()))
                break;
            if (inst->isUncond())
            {
                auto branch = static_cast<UncondBrInstruction *>(inst)->getBranch();
                if (branch == nullptr)
                    break;
                static_cast<UncondBrInstruction *>(inst)->setBranch(blk2blk[branch]);
            }
            if (inst->isCond())
            {
                auto trueBranch = static_cast<CondBrInstruction *>(inst)->getTrueBranch();
                auto falseBranch = static_cast<CondBrInstruction *>(inst)->getFalseBranch();
                Assert(trueBranch != nullptr && falseBranch != nullptr, "这里不应该为null");
                static_cast<CondBrInstruction *>(inst)->setTrueBranch(blk2blk[trueBranch]);
                static_cast<CondBrInstruction *>(inst)->setFalseBranch(blk2blk[falseBranch]);
            }
        }
    }
#ifdef INLINEDEBUG
    std::queue<BasicBlock *> q;
    std::set<BasicBlock *> color;
    q.push(entryBlock);
    color.insert(entryBlock);
    while (!q.empty())
    {
        auto nowB = q.front();
        q.pop();
        if (nowB == entryBlock)
            nowB->output();
        for (auto succ = nowB->succ_begin(); succ != nowB->succ_end(); succ++)
            if (!color.count(*succ))
            {
                q.push(*succ);
                color.insert(*succ);
            }
    }
#endif
}

void FunctionInline::merge(Function *func, Instruction *callInst)
{
    // entryBlock中如果有alloc，移到func的entry中吧
    std::vector<Instruction *> movList;
    auto entry = func->getEntry();
    for (auto inst = entryBlock->begin(); inst != entryBlock->end(); inst = inst->getNext())
        if (inst->isAlloc())
            movList.push_back(inst);
    for (auto inst : movList)
    {
        entryBlock->remove(inst);
        entry->insertFront(inst, static_cast<AllocaInstruction *>(inst)->getEntry()->getType()->isArray());
    }
    // 把callInst之后的指令全部移到tailB中
    auto headB = callInst->getParent();
    auto tailB = new BasicBlock(func);
    auto tailHead = tailB->end();
    tailHead->setNext(callInst->getNext());
    callInst->getNext()->setPrev(tailHead);
    auto tailEnd = headB->rbegin();
    tailEnd->setNext(tailHead);
    tailHead->setPrev(tailEnd);
    for (auto inst = tailB->begin(); inst != tailB->end(); inst = inst->getNext())
        inst->setParent(tailB);
    callInst->setNext(headB->end());
    headB->end()->setPrev(callInst);
    // tailB继承headB后继关系
    for (auto succ = headB->succ_begin(); succ != headB->succ_end(); succ++)
    {
        (*succ)->addPred(tailB);
        tailB->addSucc(*succ);
        for (auto inst = (*succ)->begin(); inst != (*succ)->end(); inst = inst->getNext())
        {
            if (!inst->isPhi())
                break;
            auto &srcs = static_cast<PhiInstruction *>(inst)->getSrcs();
            if (srcs.find(headB) != srcs.end())
            {
                auto op = srcs[headB];
                srcs.erase(headB);
                srcs.insert(std::make_pair(tailB, op));
            }
        }
    }
    // headB设置后继关系
    auto retValue = callInst->getDef();
    headB->remove(callInst);
    new UncondBrInstruction(entryBlock, headB);
    headB->cleanAllSucc();
    headB->addSucc(entryBlock);
    entryBlock->addPred(headB);
    // tailB设置前驱关系
    PhiInstruction *phiInst = nullptr;
    if (retValue != nullptr)
    {
        phiInst = new PhiInstruction(retValue, nullptr);
        retValue->setDef(phiInst);
        tailB->insertFront(phiInst, false);
    }
    for (auto pa : exitBlocks)
    {
        auto pred = pa.first;
        auto inst = pa.second.first;
        auto retOp = pa.second.second;
        pred->addSucc(tailB);
        tailB->addPred(pred);
        static_cast<UncondBrInstruction *>(inst)->setBranch(tailB);
        if (phiInst != nullptr)
            phiInst->addSrc(pred, retOp);
    }
}

void FunctionInline::doInline(Function *func)
{
    for (auto calledInst : callIns[func])
    {
        auto callerFunc = calledInst->getParent()->getParent();
        if (inlinedFuncs.count(callerFunc))
            continue;
        // 把原来的函数复制一份
        copyFunc(calledInst, func);
        // 合并
        merge(callerFunc, calledInst);
    }
    inlinedFuncs.insert(func);
    // for (auto &pa : callIns)
    // {
    //     if (pa.first == func)
    //         continue;
    //     auto calls = pa.second;
    //     for (auto inst : calls)
    //         if (inst->getParent()->getParent() == func)
    //             pa.second.erase(inst);
    // }
}

void FunctionInline::pass()
{
    Log("Function Inline start");
    clearData();
    preProcess();
    std::set<Function *> removeList;
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        if ((*func) != unit->getMain() && shouldBeInlined(*func))
        {
            doInline(*func);
            removeList.insert(*func);
        }
    }
    for (auto fun : removeList)
        unit->removeFunc(fun);
    Log("Function Inline over");
}