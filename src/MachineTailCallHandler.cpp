#include "MachineTailCallHandler.h"

void MachineTailCallHandler::pass()
{
    for (auto func = munit->begin(); func != munit->end(); func++)
    {
        for (auto mb : (*func)->getBlocks())
        {
            for (auto minst = mb->rbegin(); minst != mb->rend(); minst++)
            {
                auto inst = *minst;
                // 发现尾调用
                if (inst->isCall() && static_cast<BranchMInstruction *>(inst)->getTailCall())
                {
                    assert((*mb->rbegin())->isRet()); // 最后一条应该是bx
                    auto target = inst->getUse()[0];
                    mb->eraseInst(inst);
                    auto bInst = new BranchMInstruction(mb, BranchMInstruction::B, target);
                    bInst->setIsTailCall(true);
                    mb->getInsts()[mb->getInsts().size() - 1] = bInst;
                    break;
                }
            }
        }
    }
}
