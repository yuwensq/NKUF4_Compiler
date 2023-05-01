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

bool MachineCopyProp::couldKill(MachineInstruction *minst, CopyStmt &cs)
{
    if (minst->getDef().size() > 0)
    {
        auto def = minst->getDef()[0];
        return (*cs.dst == *def || *cs.src == *def);
    }

    auto isReg = [&](MachineOperand *op, int regNo, bool fpu)
    {
        bool isr = (op->isReg() && op->getReg() == regNo);
        return (fpu ? (isr && op->isFReg()) : isr);
    };
    if (minst->isCall())
    {
        for (int i = 0; i < 4; i++)
            if (isReg(cs.dst, i, false) || isReg(cs.src, i, false))
                return true;
        for (int i = 0; i < 16; i++)
            if (isReg(cs.dst, i, true) || isReg(cs.src, i, true))
                return true;
    }
    return false;
}

int MachineCopyProp::getHash(MachineOperand *op)
{
    /* 普通的虚拟寄存器直接返回编号，r系列的rx哈希为-x-33，s系列的sx哈希为-x-1 */
    if (op->isImm() || op->isLabel())
        return 0;
    int res = 0;
    res = op->getReg();
    if (op->isReg())
    {
        if (!op->isFReg())
            res = -res - 33;
        else
            res = -res - 1;
    }
    return res;
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
                std::set<int> gen = Gen[mb];
                for (auto index : gen)
                {
                    auto &cs = allCopyStmts[index];
                    if (couldKill(minst, cs))
                    {
                        Gen[mb].erase(index);
                    }
                }
            }
            // 这里不要cond mov吧，cond mov应该影响不大
            if ((minst->isMov() || minst->isVMov32()) && !minst->isCondMov())
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
                for (int i = 0; i < allCopyStmts.size(); i++)
                {
                    if (couldKill(minst, allCopyStmts[i]))
                        Kill[mb].insert(i);
                }
            }
            if ((minst->isMov() || minst->isVMov32()) && !minst->isCondMov())
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
                res = false;

            if (minst->getDef().size() > 0 || minst->isCall())
            {
                std::set<int> in = In[mb];
                for (auto index : in)
                {
                    auto &cs = allCopyStmts[index];
                    if (couldKill(minst, cs))
                    {
                        In[mb].erase(index);
                        op2Src.erase(getHash(cs.dst));
                    }
                }
            }

            if ((minst->isMov() || minst->isVMov32()) && !minst->isCondMov())
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
    Log("汇编复制传播结束\n");
}