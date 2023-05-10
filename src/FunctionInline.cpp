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

void FunctionInline::copyFunc(Function *callerFunc, Function *calleeFunc)
{
    std::map<BasicBlock *, BasicBlock *> blk2blk;
    std::map<Operand *, Operand *> op2op;
    blk2blk.clear();
    op2op.clear();
    for (auto bb : calleeFunc->getBlockList())
    {
        auto newBB = new BasicBlock(callerFunc);
        if (bb == callerFunc->getEntry())
            entryBlock = newBB;
        blk2blk[bb] = newBB;

        for (auto inst = bb->begin(); inst != bb->end(); inst = inst->getNext())
        {
            Instruction *newInst = nullptr;
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
                continue;
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
        copyFunc(callerFunc, func);
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