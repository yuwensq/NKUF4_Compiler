#include <algorithm>
#include "LinearScan.h"
#include "SymbolTable.h"
#include "AsmBuilder.h"
#include "MachineCode.h"
#include "LiveVariableAnalysis.h"

LinearScan::LinearScan(MachineUnit *unit)
{
    this->unit = unit;
    for (int i = 4; i < 11; i++)
        regs.push_back(i);
    for (int i = 16; i <= 31; i++) // s0-s15好像都可以用来传参
        fpuRegs.push_back(i);
}

void LinearScan::allocateRegisters()
{
    for (auto &f : unit->getFuncs())
    {
        func = f;
        bool success;
        success = false;
        while (!success) // repeat until all vregs can be mapped
        {
            computeLiveIntervals();
            spillIntervals.clear();
            success = linearScanRegisterAllocation();
            if (success) // all vregs can be mapped to real regs
                modifyCode();
            else // spill vregs that can't be mapped to real regs
                genSpillCode();
        }
    }
}

void LinearScan::makeDuChains()
{
    LiveVariableAnalysis lva;
    lva.pass(func);
    du_chains.clear();
    int i = 0;
    std::map<MachineOperand, std::set<MachineOperand *>> liveVar;
    for (auto &bb : func->getBlocks())
    {
        liveVar.clear();
        for (auto &t : bb->getLiveOut())
            liveVar[*t].insert(t);
        int no;
        no = i = bb->getInsts().size() + i;
        for (auto inst = bb->getInsts().rbegin(); inst != bb->getInsts().rend(); inst++)
        {
            (*inst)->setNo(no--);
            for (auto &def : (*inst)->getDef())
            {
                if (def->isVReg())
                {
                    auto &uses = liveVar[*def];
                    du_chains[def].insert(uses.begin(), uses.end());
                    auto &kill = lva.getAllUses()[*def];
                    std::set<MachineOperand *> res;
                    set_difference(uses.begin(), uses.end(), kill.begin(), kill.end(), inserter(res, res.end()));
                    liveVar[*def] = res;
                }
            }
            for (auto &use : (*inst)->getUse())
            {
                if (use->isVReg())
                    liveVar[*use].insert(use);
            }
        }
    }
}

void LinearScan::computeLiveIntervals()
{
    makeDuChains();
    intervals.clear();
    for (auto &du_chain : du_chains)
    {
        int t = -1;
        for (auto &use : du_chain.second)
            t = std::max(t, use->getParent()->getNo());
        Interval *interval = new Interval({du_chain.first->getParent()->getNo(), t, false, 0, 0, {du_chain.first}, du_chain.second, du_chain.first->isFReg()});
        intervals.push_back(interval);
    }
    for (auto &interval : intervals)
    {
        auto uses = interval->uses;
        auto begin = interval->start;
        auto end = interval->end;
        for (auto block : func->getBlocks())
        {
            auto liveIn = block->getLiveIn();
            auto liveOut = block->getLiveOut();
            bool in = false;
            bool out = false;
            for (auto use : uses)
                if (liveIn.count(use))
                {
                    in = true;
                    break;
                }
            for (auto use : uses)
                if (liveOut.count(use))
                {
                    out = true;
                    break;
                }
            if (in && out)
            {
                begin = std::min(begin, (*(block->begin()))->getNo());
                end = std::max(end, (*(block->rbegin()))->getNo());
            }
            else if (!in && out)
            {
                for (auto i : block->getInsts())
                    if (i->getDef().size() > 0 &&
                        i->getDef()[0] == *(uses.begin()))
                    {
                        begin = std::min(begin, i->getNo());
                        break;
                    }
                end = std::max(end, (*(block->rbegin()))->getNo());
            }
            else if (in && !out)
            {
                begin = std::min(begin, (*(block->begin()))->getNo());
                int temp = 0;
                for (auto use : uses)
                    if (use->getParent()->getParent() == block)
                        temp = std::max(temp, use->getParent()->getNo());
                end = std::max(temp, end);
            }
        }
        interval->start = begin;
        interval->end = end;
    }
    bool change;
    change = true;
    while (change)
    {
        change = false;
        std::vector<Interval *> t(intervals.begin(), intervals.end());
        for (size_t i = 0; i < t.size(); i++)
            for (size_t j = i + 1; j < t.size(); j++)
            {
                Interval *w1 = t[i];
                Interval *w2 = t[j];
                if (**w1->defs.begin() == **w2->defs.begin())
                {
                    std::set<MachineOperand *> temp;
                    set_intersection(w1->uses.begin(), w1->uses.end(), w2->uses.begin(), w2->uses.end(), inserter(temp, temp.end()));
                    if (!temp.empty())
                    {
                        change = true;
                        w1->defs.insert(w2->defs.begin(), w2->defs.end());
                        w1->uses.insert(w2->uses.begin(), w2->uses.end());
                        // w1->start = std::min(w1->start, w2->start);
                        // w1->end = std::max(w1->end, w2->end);
                        auto w1Min = std::min(w1->start, w1->end);
                        auto w1Max = std::max(w1->start, w1->end);
                        auto w2Min = std::min(w2->start, w2->end);
                        auto w2Max = std::max(w2->start, w2->end);
                        w1->start = std::min(w1Min, w2Min);
                        w1->end = std::max(w1Max, w2Max);
                        auto it = std::find(intervals.begin(), intervals.end(), w2);
                        if (it != intervals.end())
                            intervals.erase(it);
                    }
                }
            }
    }
    sort(intervals.begin(), intervals.end(), compareStart);
}

bool LinearScan::linearScanRegisterAllocation()
{
    // Todo
    active.clear();
    regs.clear();
    fpuRegs.clear();
    for (int i = 4; i < 11; i++)
        regs.push_back(i);
    for (int i = 16; i <= 31; i++)
        fpuRegs.push_back(i);
    bool result = true;
    for (auto interval : intervals)
    {
        expireOldIntervals(interval);
        if ((interval->fpu && fpuRegs.empty()) || (!interval->fpu && regs.empty()))
        {
            spillAtInterval(interval);
            result = false;
        }
        else
        {
            if (interval->fpu)
            {
                interval->rreg = fpuRegs[0];
                fpuRegs.erase(fpuRegs.begin());
            }
            else
            {
                interval->rreg = regs[0];
                regs.erase(regs.begin());
            }
            active.push_back(interval);
            sort(active.begin(), active.end(), compareEnd);
        }
    }
    return result;
}

void LinearScan::expireOldIntervals(Interval *interval)
{
    // Todo
    std::vector<Interval *>::iterator it = active.begin();
    while (it != active.end())
    {
        Interval *actInterval = *it;
        if (actInterval->end >= interval->start)
        {
            break;
        }
        if (actInterval->fpu)
            fpuRegs.push_back(actInterval->rreg);
        else
            regs.push_back(actInterval->rreg);
        it = active.erase(it);
    }
    // sort(regs.begin(), regs.end());
}

void LinearScan::spillAtInterval(Interval *interval)
{
    // Todo
    auto it = active.rbegin();
    for (; it != active.rend(); it++)
    {
        if ((*it)->fpu == interval->fpu)
            break;
    }
    if ((*it)->end > interval->end)
    {
        interval->rreg = (*it)->rreg;
        (*it)->spill = true;
        spillIntervals.push_back(*it);
        active.erase((++it).base());
        active.push_back(interval);
        sort(active.begin(), active.end(), compareEnd);
    }
    else
    {
        interval->spill = true;
        spillIntervals.push_back(interval);
    }
}

void LinearScan::modifyCode()
{
    for (auto &interval : intervals)
    {
        func->addSavedRegs(interval->rreg);
        for (auto def : interval->defs)
            def->setReg(interval->rreg);
        for (auto use : interval->uses)
            use->setReg(interval->rreg);
    }
}

void LinearScan::genSpillCode()
{
    for (auto &interval : spillIntervals)
    {
        if (!interval->spill)
            continue;
        // TODO
        /* HINT:
         * The vreg should be spilled to memory.
         * 1. insert ldr inst before the use of vreg
         * 2. insert str inst after the def of vreg
         */
        // 在栈中分配内存
        interval->disp = func->AllocSpace(4) * -1;
        // 在use前插入ldr
        for (auto use : interval->uses)
        {
            auto cur_bb = use->getParent()->getParent();
            if (interval->disp > -254)
            {
                MachineInstruction *cur_inst = nullptr;
                if (interval->fpu)
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::VLDR,
                                                    new MachineOperand(*use),
                                                    new MachineOperand(MachineOperand::REG, 11),
                                                    new MachineOperand(MachineOperand::IMM, interval->disp));
                else
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::LDR,
                                                    new MachineOperand(*use),
                                                    new MachineOperand(MachineOperand::REG, 11),
                                                    new MachineOperand(MachineOperand::IMM, interval->disp));
                cur_bb->insertBefore(cur_inst, use->getParent());
            }
            // 这里负数太小的话就load吧
            else
            {
                auto internal_reg = new MachineOperand(MachineOperand::VREG, SymbolTable::getLabel());
                MachineInstruction *cur_inst = new LoadMInstruction(cur_bb,
                                                                    LoadMInstruction::LDR,
                                                                    internal_reg,
                                                                    new MachineOperand(MachineOperand::IMM, interval->disp));
                cur_bb->insertBefore(cur_inst, use->getParent());
                cur_inst = new BinaryMInstruction(cur_bb,
                                                  BinaryMInstruction::ADD,
                                                  new MachineOperand(*internal_reg),
                                                  new MachineOperand(*internal_reg),
                                                  new MachineOperand(MachineOperand::REG, 11));
                cur_bb->insertBefore(cur_inst, use->getParent());
                if (interval->fpu)
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::VLDR,
                                                    new MachineOperand(*use),
                                                    new MachineOperand(*internal_reg));
                else
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::LDR,
                                                    new MachineOperand(*use),
                                                    new MachineOperand(*internal_reg));
                cur_bb->insertBefore(cur_inst, use->getParent());
            }
        }
        // 在def后插入str
        for (auto def : interval->defs)
        {
            auto cur_bb = def->getParent()->getParent();
            if (interval->disp > -254)
            {
                MachineInstruction *cur_inst = nullptr;
                if (interval->fpu)
                    cur_inst = new StoreMInstruction(cur_bb,
                                                     StoreMInstruction::VSTR,
                                                     new MachineOperand(*def),
                                                     new MachineOperand(MachineOperand::REG, 11),
                                                     new MachineOperand(MachineOperand::IMM, interval->disp));
                else
                    cur_inst = new StoreMInstruction(cur_bb,
                                                     StoreMInstruction::STR,
                                                     new MachineOperand(*def),
                                                     new MachineOperand(MachineOperand::REG, 11),
                                                     new MachineOperand(MachineOperand::IMM, interval->disp));
                cur_bb->insertAfter(cur_inst, def->getParent());
            }
            // 这里负数太小的话就load吧
            else
            {
                auto internal_reg = new MachineOperand(MachineOperand::VREG, SymbolTable::getLabel());
                auto cur_inst = new LoadMInstruction(cur_bb,
                                                     LoadMInstruction::LDR,
                                                     internal_reg,
                                                     new MachineOperand(MachineOperand::IMM, interval->disp));
                cur_bb->insertAfter(cur_inst, def->getParent());
                auto cur_inst1 = new BinaryMInstruction(cur_bb,
                                                        BinaryMInstruction::ADD,
                                                        new MachineOperand(*internal_reg),
                                                        new MachineOperand(*internal_reg),
                                                        new MachineOperand(MachineOperand::REG, 11));
                cur_bb->insertAfter(cur_inst1, cur_inst);

                MachineInstruction *cur_inst2 = nullptr;
                if (interval->fpu)
                    cur_inst2 = new StoreMInstruction(cur_bb,
                                                      StoreMInstruction::VSTR,
                                                      new MachineOperand(*def),
                                                      new MachineOperand(*internal_reg));
                else
                    cur_inst2 = new StoreMInstruction(cur_bb,
                                                      StoreMInstruction::STR,
                                                      new MachineOperand(*def),
                                                      new MachineOperand(*internal_reg));
                cur_bb->insertAfter(cur_inst2, cur_inst1);
            }
        }
    }
}

bool LinearScan::compareStart(Interval *a, Interval *b)
{
    return a->start < b->start;
}

bool LinearScan::compareEnd(Interval *a, Interval *b)
{
    return a->end < b->end;
}