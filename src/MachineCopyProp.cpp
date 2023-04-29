#include "MachineCopyProp.h"
#include "Type.h"

void MachineCopyProp::clearData()
{
    allCopyStmts.clear();
    inst2CopyStmt.clear();
    Gen.clear();
    Kill.clear();
    In.clear();
    Out.clear();
}

void MachineCopyProp::calGenKill(MachineFunction *func)
{
    // cal gen
    for (auto mb : func->getBlocks())
    {
        for (auto minst : mb->getInsts())
        {
            if (minst->isMov() || minst->isVMov32())
            {
                CopyStmt cs(minst);
                auto it = find(allCopyStmts.begin(), allCopyStmts.end(), cs);
                int index = it - allCopyStmts.end();
                inst2CopyStmt[minst] = index;
                if (it == allCopyStmts.end())
                    allCopyStmts.push_back(cs);
                Gen[mb].insert(index);
            }
            else
            {
                if (minst->getDef().size() > 0 || minst->isCall())
                {
                    MachineOperand *def = nullptr;
                    if (minst->isCall())
                    {
                        auto funcName = minst->getUse()[0]->getLabel();
                        funcName = funcName.substr(1, funcName.size() - 1);
                        auto se = globals->lookup(funcName);
                        def = new MachineOperand(MachineOperand::REG, 0, static_cast<FunctionType *>(se->getType())->getRetType()->isFloat());
                    }
                    else
                        def = minst->getDef()[0];
                    std::set<int> gen = Gen[mb];
                    for (auto index : gen)
                    {
                        auto &cs = allCopyStmts[index];
                        if (*cs.dst == *def || *cs.src == *def)
                        {
                            Gen[mb].erase(index);
                        }
                    }
                    if (minst->isCall())
                        delete def;
                }
            }
        }
    }
    // cal kill
    for (auto mb : func->getBlocks())
    {
        for (auto minst : mb->getInsts())
        {
            if (minst->isMov() || minst->isVMov32())
            {
                Kill[mb].erase(inst2CopyStmt[minst]);
            }
            else
            {
                if (minst->getDef().size() > 0 || minst->isCall())
                {
                    MachineOperand *def = nullptr;
                    if (minst->isCall())
                    {
                        auto funcName = minst->getUse()[0]->getLabel();
                        funcName = funcName.substr(1, funcName.size() - 1);
                        auto se = globals->lookup(funcName);
                        def = new MachineOperand(MachineOperand::REG, 0, static_cast<FunctionType *>(se->getType())->getRetType()->isFloat());
                    }
                    else
                        def = minst->getDef()[0];
                    for (int i = 0; i < allCopyStmts.size(); i++)
                    {
                        if (*allCopyStmts[i].dst == *def || *allCopyStmts[i].src == *def)
                            Kill[mb].insert(i);
                    }
                    if (minst->isCall())
                        delete def;
                }
            }
        }
    }
}

void MachineCopyProp::calInOut(MachineFunction *func)
{
}

bool MachineCopyProp::replaceOp(MachineFunction *func)
{
    return false;
}

/**
 * 返回true表示结束了，返回false表示还没有收敛
 */
bool MachineCopyProp::copyProp(MachineFunction *func)
{
    clearData();
    calGenKill(func);
    calInOut(func);
    return replaceOp(func);
}

void MachineCopyProp::pass()
{
    for (auto func = munit->begin(); func != munit->end(); func++)
    {
        while (!copyProp(*func))
            ;
    }
}