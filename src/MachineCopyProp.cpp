#include "MachineCopyProp.h"
#include "Type.h"
#include <unordered_set>

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

int MachineCopyProp::getHash(MachineOperand *op)
{
    if (op->isImm() || op->isLabel())
        return 0;
    int res = 0;
    res = op->getReg();
    if (op->isReg())
        res = -res - 1;
    return res;
}

int MachineCopyProp::hasReturn(MachineInstruction *minst)
{
    auto funcName = minst->getUse()[0]->getLabel();
    funcName = funcName.substr(1, funcName.size() - 1);
    auto se = globals->lookup(funcName);
    if (se == nullptr)
        return 0;
    auto retType = static_cast<FunctionType *>(se->getType())->getRetType();
    if (retType->isVoid())
        return 0;
    if (retType->isInt())
        return 1;
    else if (retType->isFloat())
        return 2;
    return 0;
}

void MachineCopyProp::calGenKill(MachineFunction *func)
{
    // cal gen
    for (auto mb : func->getBlocks())
    {
        for (auto minst : mb->getInsts())
        {
            if (minst->getDef().size() > 0 || minst->isCall())
            {
                MachineOperand *def = nullptr;
                int retType = 0;
                if (minst->isCall())
                {
                    retType = hasReturn(minst);
                    if (retType == 0)
                        continue;
                }
                else
                    def = minst->getDef()[0];
                std::set<int> gen = Gen[mb];
                for (auto index : gen)
                {
                    auto &cs = allCopyStmts[index];
                    if (!minst->isCall() && (*cs.dst == *def || *cs.src == *def))
                    {
                        Gen[mb].erase(index);
                    }
                    else if (minst->isCall())
                    {
                    }
                }
            }
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
        }
    }
    // cal kill
    for (auto mb : func->getBlocks())
    {
        for (auto minst : mb->getInsts())
        {
            if (minst->getDef().size() > 0 || minst->isCall())
            {
                MachineOperand *def = nullptr;
                if (minst->isCall())
                {
                    def = getDefReg(minst);
                    if (def == nullptr)
                        continue;
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
            if (minst->isMov() || minst->isVMov32())
            {
                Kill[mb].erase(inst2CopyStmt[minst]);
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
    bool res = true;
    for (auto mb : func->getBlocks())
    {
        std::unordered_map<int, MachineOperand *> op2Src;
        for (auto index : In[mb])
        {
            op2Src[getHash(allCopyStmts[index].dst)] = allCopyStmts[index].src;
        }
        for (auto minst : mb->getInsts())
        {
            auto uses = minst->getUse();
            bool change = false;
            for (auto use : uses)
            {
                if (use->isImm() || use->isLabel())
                    continue;
                if (op2Src.find(getHash(use)) != op2Src.end())
                {
                    auto new_use = new MachineOperand(*op2Src[getHash(use)]);
                    change = minst->replaceUse(use, new_use);
                }
            }
            if (change)
            {
                res = false;
                continue;
            }
            if (minst->getDef().size() > 0 || minst->isCall())
            {
                MachineOperand *def = nullptr;
                if (minst->isCall())
                {
                    def = getDefReg(minst);
                    if (def == nullptr)
                        continue;
                }
                else
                    def = minst->getDef()[0];
                std::set<int> in = In[mb];
                for (auto index : in)
                {
                    auto &cs = allCopyStmts[index];
                    if (*cs.dst == *def || *cs.src == *def)
                    {
                        In[mb].erase(index);
                        op2Src.erase(getHash(cs.dst));
                    }
                }
                if (minst->isCall())
                    delete def;
            }
            if (minst->isMov() || minst->isVMov32())
            {
                In[mb].insert(inst2CopyStmt[minst]);
                op2Src[getHash(minst->getDef()[0])] = minst->getUse()[0];
            }
        }
    }
    return res;
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
    Log("汇编复制传播开始");
    int num = 0;
    for (auto func = munit->begin(); func != munit->end(); func++)
    {
        while (!copyProp(*func))
            Log("pass%dover", ++num);
    }
    Log("汇编复制传播结束");
}