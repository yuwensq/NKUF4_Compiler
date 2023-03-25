#include "MachinePeepHole.h"
#include "debug.h"

void MachinePeepHole::pass()
{
    Log("窥孔优化开始\n");
    for (auto func : unit->getFuncs())
    {
        for (auto blk : func->getBlocks())
        {
            for (auto now_inst = blk->begin(); now_inst != blk->end(); now_inst++)
            {
                if (now_inst + 1 == blk->end())
                    break;
                auto next_inst = now_inst + 1;
                if ((*now_inst)->isCondMov() && (*next_inst)->isCondMov() && *(*now_inst)->getDef()[0] == *(*next_inst)->getDef()[0])
                {
                    auto third_inst = next_inst + 1;
                    if (third_inst == blk->end() || !(*(*third_inst)->getUse()[0] == *(*now_inst)->getDef()[0]))
                    {
                        now_inst = blk->getInsts().erase(now_inst);
                        now_inst = blk->getInsts().erase(now_inst);
                        now_inst--;
                    }
                }
                else if ((*next_inst)->isMov() && (*now_inst)->getDef().size() > 0 && *(*now_inst)->getDef()[0] == *(*next_inst)->getUse()[0])
                {
                    (*now_inst)->getDef()[0] = (*next_inst)->getDef()[0];
                    (*now_inst)->getDef()[0]->setParent(*now_inst);
                    blk->getInsts().erase(next_inst);
                }
                else if ((*now_inst)->isMov() && (*next_inst)->getUse().size() > 0)
                {
                    if (*(*now_inst)->getDef()[0] == *(*next_inst)->getUse()[0] && !(*now_inst)->getUse()[0]->isImm())
                    {
                        (*next_inst)->getUse()[0] = (*now_inst)->getUse()[0];
                        (*next_inst)->getUse()[0]->setParent(*next_inst);
                        blk->getInsts().erase(now_inst);
                        now_inst--;
                    }
                }
                else if ((*next_inst)->isCLoad() && (*now_inst)->isCStore())
                {
                    Log("ldr after str");
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
                    if (*(*now_inst)->getDef()[0] == *(*next_inst)->getUse()[1])
                    {
                        auto dst = new MachineOperand(*((*next_inst)->getDef()[0]));
                        auto src1 = new MachineOperand(*((*now_inst)->getUse()[0]));
                        auto src2 = new MachineOperand(*((*now_inst)->getUse()[1]));
                        auto src3 = new MachineOperand(*((*next_inst)->getUse()[0]));
                        now_inst = blk->getInsts().erase(now_inst);
                        *now_inst = new MlaMInstruction(blk, dst, src1, src2, src3);
                    }
                }
            }
        }
    }
    Log("窥孔优化完成\n");
}