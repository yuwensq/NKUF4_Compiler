#include "MachinePeepHole.h"
#include "debug.h"

// #define PRINTLOG

void MachinePeepHole::pass(bool afterRegAlloc)
{
#ifdef PRINTLOG
    Log("窥孔优化开始");
#endif
    do
    {
        clearData();
        analyse();
    } while (subPass(afterRegAlloc));
#ifdef PRINTLOG
    Log("窥孔优化pass%d完成", i);
#endif
#ifdef PRINTLOG
    Log("窥孔优化完成\n");
#endif
}

bool MachinePeepHole::subPass(bool afterRegAlloc)
{
    bool changed = false;
    for (auto func : unit->getFuncs())
    {
        for (auto blk : func->getBlocks())
        {
            bool has_uncondbr = false;
            for (auto now_inst = blk->begin(); now_inst != blk->end(); now_inst++)
            {
                // 删除出现在无条件跳转后的代码
                if (has_uncondbr)
                {
                    blk->getInsts().erase(now_inst);
                    now_inst--;
                    continue;
                }
                else if ((*now_inst)->isUncondBranch())
                {
                    has_uncondbr = true;
                    continue;
                }

                // 进行指令的调整
                if (now_inst + 1 == blk->end())
                    break;

                auto next_inst = now_inst + 1;
                if (((*next_inst)->isVMov32() || (*next_inst)->isMov()) && (*now_inst)->getDef().size() > 0 && *(*now_inst)->getDef()[0] == *(*next_inst)->getUse()[0] && op2DefTimes[*(*now_inst)->getDef()[0]] == 1 && op2UseTimes[*(*now_inst)->getDef()[0]] == 1)
                {
                    // add r0, r1, r2
                    // mov r3, r0
                    // => add r3, r1, r2
                    changed = true;
                    (*now_inst)->getDef()[0]->setParent(nullptr);
                    (*now_inst)->getDef()[0] = new MachineOperand(*(*next_inst)->getDef()[0]);
                    (*now_inst)->getDef()[0]->setParent(*now_inst);
                    now_inst = blk->getInsts().erase(next_inst);
                    now_inst--;
                    if (now_inst != blk->begin())
                        now_inst--;
                }
                else if ((*now_inst)->isAdd() && (*now_inst)->getUse()[1]->isImm() && (((*next_inst)->isVStore() && (*now_inst)->getUse()[1]->getVal() <= 1020 && (*now_inst)->getUse()[1]->getVal() >= 0 && (*now_inst)->getUse()[1]->getVal() % 4 == 0) || ((*next_inst)->isCStore() && (*now_inst)->getUse()[1]->getVal() < 4096)) && (*next_inst)->getUse().size() == 2 && *(*next_inst)->getUse()[1] == *(*now_inst)->getDef()[0] && op2UseTimes[*(*now_inst)->getDef()[0]] == 1)
                {
                    auto strInst = *next_inst;
                    strInst->getUse()[1]->setParent(nullptr);
                    strInst->getUse()[1] = new MachineOperand(*(*now_inst)->getUse()[0]);
                    strInst->getUse().push_back(new MachineOperand(*(*now_inst)->getUse()[1]));
                    strInst->getUse()[1]->setParent(strInst);
                    strInst->getUse()[2]->setParent(strInst);
                    now_inst = blk->getInsts().erase(now_inst);
                    now_inst--;
                }
                else if ((*now_inst)->isAdd() && (*now_inst)->getUse()[1]->isImm() && (next_inst + 1) != blk->end() && !((*next_inst)->getDef().size() == 1 && *(*next_inst)->getDef()[0] == *(*now_inst)->getUse()[0]) && (((*(next_inst + 1))->isVStore() && (*now_inst)->getUse()[1]->getVal() <= 1020 && (*now_inst)->getUse()[1]->getVal() >= 0 && (*now_inst)->getUse()[1]->getVal() % 4 == 0) || ((*(next_inst + 1))->isCStore() && (*now_inst)->getUse()[1]->getVal() < 4096)) && (*(next_inst + 1))->getUse().size() == 2 && *(*(next_inst + 1))->getUse()[1] == *(*now_inst)->getDef()[0] && op2UseTimes[*(*now_inst)->getDef()[0]] == 1)
                {
                    auto strInst = *(next_inst + 1);
                    strInst->getUse()[1]->setParent(nullptr);
                    strInst->getUse()[1] = new MachineOperand(*(*now_inst)->getUse()[0]);
                    strInst->getUse().push_back(new MachineOperand(*(*now_inst)->getUse()[1]));
                    strInst->getUse()[1]->setParent(strInst);
                    strInst->getUse()[2]->setParent(strInst);
                    now_inst = blk->getInsts().erase(now_inst);
                    now_inst--;
                }
                else if ((*now_inst)->isAdd() && (*now_inst)->getUse()[1]->isImm() && (((*next_inst)->isVLoad() && (*now_inst)->getUse()[1]->getVal() <= 1020 && (*now_inst)->getUse()[1]->getVal() >= 0 && (*now_inst)->getUse()[1]->getVal() % 4 == 0) || ((*next_inst)->isCLoad() && (*now_inst)->getUse()[1]->getVal() < 4096)) && (*next_inst)->getUse().size() == 1 && *(*next_inst)->getUse()[0] == *(*now_inst)->getDef()[0] && op2UseTimes[*(*now_inst)->getDef()[0]] == 1)
                {
                    auto ldrInst = *next_inst;
                    ldrInst->getUse()[0]->setParent(nullptr);
                    ldrInst->getUse()[0] = new MachineOperand(*(*now_inst)->getUse()[0]);
                    ldrInst->getUse().push_back(new MachineOperand(*(*now_inst)->getUse()[1]));
                    ldrInst->getUse()[0]->setParent(ldrInst);
                    ldrInst->getUse()[1]->setParent(ldrInst);
                    now_inst = blk->getInsts().erase(now_inst);
                    now_inst--;
                }
                else if ((*now_inst)->isAdd() && (*now_inst)->getUse()[1]->isImm() && (next_inst + 1) != blk->end() && !((*next_inst)->getDef().size() == 1 && *(*next_inst)->getDef()[0] == *(*now_inst)->getUse()[0]) && (((*(next_inst + 1))->isVLoad() && (*now_inst)->getUse()[1]->getVal() <= 1020 && (*now_inst)->getUse()[1]->getVal() >= 0 && (*now_inst)->getUse()[1]->getVal() % 4 == 0) || ((*(next_inst + 1))->isCLoad() && (*now_inst)->getUse()[1]->getVal() < 4096)) && (*(next_inst + 1))->getUse().size() == 1 && *(*(next_inst + 1))->getUse()[0] == *(*now_inst)->getDef()[0] && op2UseTimes[*(*now_inst)->getDef()[0]] == 1)
                {
                    auto ldrInst = *(next_inst + 1);
                    ldrInst->getUse()[0]->setParent(nullptr);
                    ldrInst->getUse()[0] = new MachineOperand(*(*now_inst)->getUse()[0]);
                    ldrInst->getUse().push_back(new MachineOperand(*(*now_inst)->getUse()[1]));
                    ldrInst->getUse()[0]->setParent(ldrInst);
                    ldrInst->getUse()[1]->setParent(ldrInst);
                    now_inst = blk->getInsts().erase(now_inst);
                    now_inst--;
                }
                // else if (!afterRegAlloc && (*now_inst)->isCondMov() && (*next_inst)->isCondMov() && *(*now_inst)->getDef()[0] == *(*next_inst)->getDef()[0])
                // {
                //     // 条件mov的结果没人用
                //     auto third_inst = next_inst + 1;
                //     if (third_inst == blk->end() || !(*(*third_inst)->getUse()[0] == *(*now_inst)->getDef()[0]))
                //     {
                //         changed = true;
                //         now_inst = blk->getInsts().erase(now_inst);
                //         now_inst = blk->getInsts().erase(now_inst);
                //         now_inst--;
                //     }
                // }
                else if (((*now_inst)->isMov() || (*now_inst)->isVMov() || (*now_inst)->isVMov32()) && *(*now_inst)->getUse()[0] == *(*now_inst)->getDef()[0])
                {
                    // mov rx, rx
                    // vmov.f32 sx, sx
                    // 如果在分配寄存器之前把mov r0, r0之类的删除掉，分配寄存器的时候会出错
                    if (!afterRegAlloc && (*now_inst)->getUse()[0]->isReg())
                        continue;
                    changed = true;
                    now_inst = blk->getInsts().erase(now_inst);
                    now_inst--;
                }
                else if ((*next_inst)->isCLoad() && (*now_inst)->isCStore())
                {
                    // str r0, [r4]
                    // ldr r1, [r4]
                    // ==> str r0, [r4]
                    //     mov r1, r0
                    if ((*now_inst)->getUse().size() - 1 == (*next_inst)->getUse().size())
                    {
                        if (*(*now_inst)->getUse()[1] == *(*next_inst)->getUse()[0])
                        {
                            bool could_chg = false;
                            if ((*next_inst)->getUse().size() <= 1)
                            {
                                could_chg = true;
                            }
                            else if (*(*now_inst)->getUse()[2] == *(*next_inst)->getUse()[1])
                            {
                                could_chg = true;
                            }
                            if (could_chg)
                            {
                                changed = true;
                                auto dst = new MachineOperand(*(*next_inst)->getDef()[0]);
                                auto src = new MachineOperand(*(*now_inst)->getUse()[0]);
                                *next_inst = new MovMInstruction(blk, MovMInstruction::MOV, dst, src);
                            }
                        }
                    }
                }
                else if ((*now_inst)->isMul() && (*next_inst)->isAdd() && op2UseTimes[*(*now_inst)->getDef()[0]] == 1)
                {
                    // mul r3, r4, r5
                    // add r6, r3, r7
                    // ==> mla r6, r4, r5, r7
                    if (*(*now_inst)->getDef()[0] == *(*next_inst)->getUse()[1])
                    {
                        changed = true;
                        auto dst = new MachineOperand(*((*next_inst)->getDef()[0]));
                        auto src1 = new MachineOperand(*((*now_inst)->getUse()[0]));
                        auto src2 = new MachineOperand(*((*now_inst)->getUse()[1]));
                        auto src3 = new MachineOperand(*((*next_inst)->getUse()[0]));
                        now_inst = blk->getInsts().erase(now_inst);
                        *now_inst = new MlaMInstruction(blk, dst, src1, src2, src3);
                    }
                    else if (*(*now_inst)->getDef()[0] == *(*next_inst)->getUse()[0] && !(*next_inst)->getUse()[1]->isImm())
                    {
                        changed = true;
                        auto dst = new MachineOperand(*((*next_inst)->getDef()[0]));
                        auto src1 = new MachineOperand(*((*now_inst)->getUse()[0]));
                        auto src2 = new MachineOperand(*((*now_inst)->getUse()[1]));
                        auto src3 = new MachineOperand(*((*next_inst)->getUse()[1]));
                        now_inst = blk->getInsts().erase(now_inst);
                        *now_inst = new MlaMInstruction(blk, dst, src1, src2, src3);
                    }
                }
                else if ((*now_inst)->isMul() && (*next_inst)->isSub())
                {
                    // mul r7, r6, r4
                    // sub r3, r5, r7
                    // ==> mls r3, r6, r4, r5
                    if (*(*now_inst)->getDef()[0] == *(*next_inst)->getUse()[1])
                    {
                        changed = true;
                        auto dst = new MachineOperand(*((*next_inst)->getDef()[0]));
                        auto src1 = new MachineOperand(*((*now_inst)->getUse()[0]));
                        auto src2 = new MachineOperand(*((*now_inst)->getUse()[1]));
                        auto src3 = new MachineOperand(*((*next_inst)->getUse()[0]));
                        now_inst = blk->getInsts().erase(now_inst);
                        *now_inst = new MlsMInstruction(blk, dst, src1, src2, src3);
                    }
                }
                else if ((*next_inst)->isVLoad() && (*now_inst)->isVStore())
                {
                    // vstr.32 s0, [r4]
                    // vldr.32 s1, [r4]
                    // ==> vstr.32 s0, [r4]
                    //     vmov.f32 r1, r0
                    if ((*now_inst)->getUse().size() - 1 == (*next_inst)->getUse().size())
                    {
                        if (*(*now_inst)->getUse()[1] == *(*next_inst)->getUse()[0])
                        {
                            bool could_chg = false;
                            if ((*next_inst)->getUse().size() <= 1)
                            {
                                could_chg = true;
                            }
                            else if (*(*now_inst)->getUse()[2] == *(*next_inst)->getUse()[1])
                            {
                                could_chg = true;
                            }
                            if (could_chg)
                            {
                                changed = true;
                                auto dst = new MachineOperand(*(*next_inst)->getDef()[0]);
                                auto src = new MachineOperand(*(*now_inst)->getUse()[0]);
                                *next_inst = new MovMInstruction(blk, MovMInstruction::VMOV32, dst, src);
                            }
                        }
                    }
                }
                else if ((*now_inst)->isMov() && !(*now_inst)->isCondMov() && (*now_inst)->getUse()[0]->isImm() && (*now_inst)->getUse()[0]->getVal() == 0)
                {
                    // mov r4, #0
                    // vmov s17, r4
                    // vsub.f32 s18, s17, s16
                    // ==> vneg.f32 s18, s16
                    if ((*next_inst)->isVMov() && *(*next_inst)->getUse()[0] == *(*now_inst)->getDef()[0])
                    {
                        auto third_inst = next_inst + 1;
                        if (third_inst != blk->getInsts().end() && (*third_inst)->isVSub() && *(*third_inst)->getUse()[0] == *(*next_inst)->getDef()[0])
                        {
                            changed = true;
                            auto dst = new MachineOperand(*(*third_inst)->getDef()[0]);
                            auto src = new MachineOperand(*(*third_inst)->getUse()[1]);
                            now_inst = blk->getInsts().erase(now_inst);
                            now_inst = blk->getInsts().erase(now_inst);
                            (*now_inst) = new VNegMInstruction(blk, dst, src);
                            now_inst--;
                        }
                    }
                }
                else if ((*now_inst)->isVMov32() && (*next_inst)->getUse().size() > 0)
                {
                    // vmov.f32 s16, s17
                    // vneg.f32 s17, s16
                    // ==> vneg.f32 s17, s17
                    // bool success = false;
                    // for (int i = 0; i < (*next_inst)->getUse().size(); i++)
                    // {
                    //     if (*(*now_inst)->getDef()[0] == *(*next_inst)->getUse()[i])
                    //     {
                    //         success = true;
                    //         (*next_inst)->getUse()[i] = new MachineOperand(*(*now_inst)->getUse()[0]);
                    //         (*next_inst)->getUse()[i]->setParent(*next_inst);
                    //     }
                    // }
                    // if (success)
                    // {
                    //     blk->getInsts().erase(now_inst);
                    //     now_inst--;
                    // }
                }
            }
        }
    }
    return changed;
}

void MachinePeepHole::clearData()
{
    op2DefTimes.clear();
    op2UseTimes.clear();
}

void MachinePeepHole::analyse()
{
    for (auto func : unit->getFuncs())
    {
        for (auto blk : func->getBlocks())
        {
            for (auto now_inst = blk->begin(); now_inst != blk->end(); now_inst++)
            {
                auto inst = *now_inst;
                for (auto def : inst->getDef())
                    op2DefTimes[*def]++;
                for (auto use : inst->getUse())
                    op2UseTimes[*use]++;
            }
        }
    }
}

void MachinePeepHole::pass2()
{
    clearData();
    analyse();
}
