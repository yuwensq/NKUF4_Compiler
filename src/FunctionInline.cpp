#include "FunctionInline.h"

#define INLINEDEBUG

void FunctionInline::clearData()
{
    callIns.clear();
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
    for (auto inst : callIns[func])
        if (inst->getParent()->getParent() == func)
            return false;
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
        if (bb == callerFunc->getEntry())
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
        for (auto pred = oldBlk->pred_begin(); pred != oldBlk->pred_end(); pred++)
        {
            newBlk->addPred(blk2blk[*pred]);
            blk2blk[*pred]->addSucc(newBlk);
        }
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
}

void FunctionInline::merge(Function *func, Instruction *callInst)
{
}

void FunctionInline::doInline(Function *func)
{
    for (auto calledInst : callIns[func])
    {
        auto callerFunc = calledInst->getParent()->getParent();
        // 把原来的函数复制一份
        copyFunc(calledInst, func);
        // 合并
        merge(callerFunc, calledInst);
    }
}

void FunctionInline::pass()
{
    Log("Function Inline start");
    clearData();
    preProcess();
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        if (shouldBeInlined(*func))
            doInline(*func);
    }
    Log("Function Inline over");
}