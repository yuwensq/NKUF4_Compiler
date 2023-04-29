#include "MachineCopyProp.h"
#include "Type.h"

// #define COPYPROPDEBUG

extern FILE *yyout;

void MachineCopyProp::clearData()
{
    allCopyStmts.clear();
    inst2CopyStmt.clear();
    Gen.clear();
    Kill.clear();
    In.clear();
    Out.clear();
}

std::set<int> MachineCopyProp::intersection(std::set<int> &a, std::set<int> &b)
{
    std::vector<int> res;
    res.resize(std::min(a.size(), b.size()));
    auto lastPos = std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), res.begin());
    return std::set<int>(res.begin(), lastPos);
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
                int index = it - allCopyStmts.begin();
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
    std::set<int> U;
    for (int i = 0; i < allCopyStmts.size(); i++)
        U.insert(i);
    auto entry = func->getBlocks()[0];
    In[entry].clear();
    Out[entry] = Gen[entry];
    for (auto mb = func->begin() + 1; mb != func->end(); mb++)
        Out[*mb] = U;

    bool outChanged = true;
    // int sum = 0;
    while (outChanged)
    {
        // sum++;
        // Log("%d", sum);
        outChanged = false;
        for (auto mb = func->begin() + 1; mb != func->end(); mb++)
        {
            // 先计算in
            std::set<int> in = U;
            for (auto preB : (*mb)->getPreds())
            {
                in = intersection(Out[preB], in);
            }
            In[*mb] = in;
            // 再计算out
            std::set<int> midDif;
            std::set<int> out;
            std::set_difference(In[*mb].begin(), In[*mb].end(), Kill[*mb].begin(), Kill[*mb].end(), inserter(midDif, midDif.begin()));
            std::set_union(Gen[*mb].begin(), Gen[*mb].end(), midDif.begin(), midDif.end(), inserter(out, out.begin()));
            if (out != Out[*mb])
            {
                outChanged = true;
                Out[*mb] = out;
            }
        }
    }
}

bool MachineCopyProp::replaceOp(MachineFunction *func)
{
    return true;
}

/**
 * 返回true表示结束了，返回false表示还没有收敛
 */
bool MachineCopyProp::copyProp(MachineFunction *func)
{
    clearData();
    calGenKill(func);
    calInOut(func);
#ifdef COPYPROPDEBUG
    fprintf(yyout, "all expr\n");
    for (auto index = 0; index < allCopyStmts.size(); index++)
    {
        fprintf(yyout, "%d ", index);
        allCopyStmts[index].dst->output();
        fprintf(yyout, " ");
        allCopyStmts[index].src->output();
        fprintf(yyout, "\n");
    }
    fprintf(yyout, "\n");
    fprintf(yyout, "gen per bb");
    for (auto mb = func->begin(); mb != func->end(); mb++)
    {
        fprintf(yyout, "\n%%B%d\n", (*mb)->getNo());
        for (auto index : Gen[*mb])
            fprintf(yyout, "    %d", index);
    }
    fprintf(yyout, "\nkill per bb");
    for (auto mb = func->begin(); mb != func->end(); mb++)
    {
        fprintf(yyout, "\n%%B%d\n", (*mb)->getNo());
        for (auto index : Kill[*mb])
            fprintf(yyout, "    %d", index);
    }
    fprintf(yyout, "\nin per bb");
    for (auto mb = func->begin(); mb != func->end(); mb++)
    {
        fprintf(yyout, "\n%%B%d\n", (*mb)->getNo());
        for (auto index : In[*mb])
            fprintf(yyout, "    %d", index);
    }
    fprintf(yyout, "\nout per bb");
    for (auto mb = func->begin(); mb != func->end(); mb++)
    {
        fprintf(yyout, "\n%%B%d\n", (*mb)->getNo());
        for (auto index : Out[*mb])
            fprintf(yyout, "    %d", index);
    }
    fprintf(yyout, "\n");
#endif
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