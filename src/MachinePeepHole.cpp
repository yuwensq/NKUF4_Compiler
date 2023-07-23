#include "MachinePeepHole.h"
#include "debug.h"

// #define PRINTLOG

void MachinePeepHole::pass(bool afterRegAlloc)
{
#ifdef PRINTLOG
    Log("窥孔优化开始");
#endif
    for (int i = 0; i < pass_times; i++)
    {
        subPass(afterRegAlloc);
#ifdef PRINTLOG
        Log("窥孔优化pass%d完成", i);
#endif
    }
#ifdef PRINTLOG
    Log("窥孔优化完成\n");
#endif
}

void MachinePeepHole::subPass(bool afterRegAlloc)
{
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
                if ((*now_inst)->isCondMov() && (*next_inst)->isCondMov() && *(*now_inst)->getDef()[0] == *(*next_inst)->getDef()[0])
                {
                    // 条件mov的结果没人用
                    auto third_inst = next_inst + 1;
                    if (third_inst == blk->end() || !(*(*third_inst)->getUse()[0] == *(*now_inst)->getDef()[0]))
                    {
                        now_inst = blk->getInsts().erase(now_inst);
                        now_inst = blk->getInsts().erase(now_inst);
                        now_inst--;
                    }
                }
                else if ((*next_inst)->isMov() && !(*now_inst)->isCondMov() && (*now_inst)->getDef().size() > 0 && *(*now_inst)->getDef()[0] == *(*next_inst)->getUse()[0])
                {
                    // add r4, r3, r2
                    // mov r5, r4
                    // ==> add r5, r3, r2
                    // (*now_inst)->getDef()[0] = (*next_inst)->getDef()[0];
                    // (*now_inst)->getDef()[0]->setParent(*now_inst);
                    // blk->getInsts().erase(next_inst);
                }
                else if ((*now_inst)->isMov() && (*next_inst)->getUse().size() > 0 && *(*now_inst)->getDef()[0] == *(*next_inst)->getUse()[0] && !(*now_inst)->getUse()[0]->isImm())
                {
                    // mov r5, r4
                    // add r8, r5, r7
                    // ==> add r8, r4, r7
                    // (*next_inst)->getUse()[0] = (*now_inst)->getUse()[0];
                    // (*next_inst)->getUse()[0]->setParent(*next_inst);
                    // blk->getInsts().erase(now_inst);
                    // now_inst--;
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
                                auto dst = new MachineOperand(*(*next_inst)->getDef()[0]);
                                auto src = new MachineOperand(*(*now_inst)->getUse()[0]);
                                *next_inst = new MovMInstruction(blk, MovMInstruction::MOV, dst, src);
                            }
                        }
                    }
                }
                else if ((*now_inst)->isMul() && (*next_inst)->isAdd())
                {
                    // mul r3, r4, r5
                    // add r6, r3, r7
                    // ==> mla r6, r4, r5, r7
                    if (*(*now_inst)->getDef()[0] == *(*next_inst)->getUse()[1])
                    {
                        auto dst = new MachineOperand(*((*next_inst)->getDef()[0]));
                        auto src1 = new MachineOperand(*((*now_inst)->getUse()[0]));
                        auto src2 = new MachineOperand(*((*now_inst)->getUse()[1]));
                        auto src3 = new MachineOperand(*((*next_inst)->getUse()[0]));
                        now_inst = blk->getInsts().erase(now_inst);
                        *now_inst = new MlaMInstruction(blk, dst, src1, src2, src3);
                    }
                    else if (*(*now_inst)->getDef()[0] == *(*next_inst)->getUse()[0] && !(*next_inst)->getUse()[1]->isImm())
                    {
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
                            auto dst = new MachineOperand(*(*third_inst)->getDef()[0]);
                            auto src = new MachineOperand(*(*third_inst)->getUse()[1]);
                            now_inst = blk->getInsts().erase(now_inst);
                            now_inst = blk->getInsts().erase(now_inst);
                            (*now_inst) = new VNegMInstruction(blk, dst, src);
                            now_inst--;
                        }
                    }
                }
                else if (((*now_inst)->isMov() || (*now_inst)->isVMov() || (*now_inst)->isVMov32()) && *(*now_inst)->getUse()[0] == *(*now_inst)->getDef()[0])
                {
                    // mov rx, rx
                    // vmov.f32 sx, sx
                    // 如果在分配寄存器之前把mov r0, r0之类的删除掉，分配寄存器的时候会出错
                    if (!afterRegAlloc && (*now_inst)->getUse()[0]->isReg())
                        continue;
                    blk->getInsts().erase(now_inst);
                    now_inst--;
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
}
