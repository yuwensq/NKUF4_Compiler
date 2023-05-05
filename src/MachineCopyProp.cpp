#include "MachineCopyProp.h"
#include "Type.h"
#include <queue>
#include <unordered_set>
#define getHash(x) MachineUnit::getHash(x)

// #define COPYPROPDEBUG

extern FILE *yyout;

void MachineCopyProp::addZeroToMov()
{
    for (auto func : munit->getFuncs())
    {
        for (auto mb : func->getBlocks())
        {
            auto it = mb->getInsts().begin();
            while (it != mb->getInsts().end())
            {
                auto minst = *it;
                if ((minst->isAdd() || minst->isSub()) && minst->getUse()[1]->isImm() && minst->getUse()[1]->getVal() == 0)
                {
                    if (*minst->getDef()[0] == *minst->getUse()[0])
                    {
                        it = mb->getInsts().erase(it);
                        continue;
                    }
                    auto movInst = new MovMInstruction(minst->getParent(), MovMInstruction::MOV, new MachineOperand(*minst->getDef()[0]), new MachineOperand(*minst->getUse()[0]));
                    *it = movInst;
                }
                it++;
            }
        }
    }
}

void MachineCopyProp::clearData()
{
    allCopyStmts.clear();
    inst2CopyStmt.clear();
    copyStmtChanged.clear();
    Gen.clear();
    Kill.clear();
    In.clear();
    Out.clear();
}

void MachineCopyProp::intersection(std::set<int> &a, std::set<int> &b, std::set<int> &out)
{
    out.clear();
    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), inserter(out, out.begin()));
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
    std::set<MachineBlock *> workList;
    for (auto mb = func->begin() + 1; mb != func->end(); mb++)
    {
        Out[*mb] = U;
        workList.insert(*mb);
    }

    while (!workList.empty())
    {
        auto mb = *workList.begin();
        workList.erase(workList.begin());
        // 先计算in
        std::set<int> in[2];
        if (mb->getPreds().size() > 0)
            in[0] = Out[mb->getPreds()[0]];
        auto it = mb->getPreds().begin() + 1;
        auto overPos = mb->getPreds().end();
        int turn = 1;
        for (it; it != overPos; it++)
        {
            intersection(Out[*it], in[turn ^ 1], in[turn]);
            turn ^= 1;
        }
        In[mb] = in[turn ^ 1];

        // 再计算out
        std::set<int> midDif;
        std::set<int> out;
        std::set_difference(In[mb].begin(), In[mb].end(), Kill[mb].begin(), Kill[mb].end(), inserter(midDif, midDif.begin()));
        std::set_union(Gen[mb].begin(), Gen[mb].end(), midDif.begin(), midDif.end(), inserter(out, out.begin()));
        if (out != Out[mb])
        {
            Out[mb] = out;
            for (auto &succ : mb->getSuccs())
                workList.insert(succ);
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
            if (!copyStmtChanged[index])
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
                if (change)
                    copyStmtChanged[inst2CopyStmt[minst]] = true;
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
    Log("cal genkill over");
    calInOut(func);
    Log("cal inout over");
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
    addZeroToMov();
    int num = 0;
    for (auto func = munit->begin(); func != munit->end(); func++)
    {
        while (!copyProp(*func))
            Log("pass%dover", ++num);
    }
    Log("汇编复制传播结束\n");
}