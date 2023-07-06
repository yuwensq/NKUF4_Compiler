#include "TailCallAnalyser.h"

bool srcIsLocalArray(Operand *op)
{
    bool res = true;
    while (dynamic_cast<GepInstruction *>(op->getDef()) != nullptr)
        op = op->getDef()->getUse()[0];
    if (dynamic_cast<AllocaInstruction *>(op->getDef()) != nullptr)
        return true;
    return false;
}

void TailCallAnalyser::pass()
{
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        for (auto bb = (*func)->begin(); bb != (*func)->end(); bb++)
        {
            // a = sum(x);
            // ret a;

            // sum(x);
            // ret void

            // a = sum(x);
            // ret void;
            auto retInst = (*bb)->rbegin();
            auto callInst = retInst->getPrev();
            if (!(retInst->isRet() && callInst->isCall()))
                continue;
            if (!(retInst->getUse().size() <= 0 || callInst->getDef() == retInst->getUse()[0]))
                continue;
            bool isTailCall = true;
            for (auto param : callInst->getUse())
            {
                if (srcIsLocalArray(param))
                {
                    isTailCall = false;
                    break;
                }
            }
            static_cast<CallInstruction *>(callInst)->setTailCall(isTailCall);
        }
    }
}
