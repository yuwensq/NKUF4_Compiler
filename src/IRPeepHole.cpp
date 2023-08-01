#include "IRPeepHole.h"
#include "Type.h"
#include "debug.h"

void IRPeepHole::subPass(Function *func)
{
    auto case1 = [&](Instruction *inst)
    {
        auto nextInst = inst->getNext();
        bool cond1 = (inst->isBinary() && inst->getOpCode() == BinaryInstruction::ADD && inst->getUse()[1]->getEntry()->isConstant());
        bool cond2 = (nextInst->isBinary() && nextInst->getOpCode() == BinaryInstruction::ADD && nextInst->getUse()[1]->getEntry()->isConstant());
        bool cond3 = cond1 && cond2 && (nextInst->getUse()[0] == inst->getDef());
        bool cond4 = cond3 && (inst->getDef()->usersNum() == 1);
        return cond4;
    };
    auto solveCase1 = [&](Instruction *inst)
    {
        auto nextInst = inst->getNext();
        inst->getUse()[0]->removeUse(inst);
        nextInst->replaceUse(nextInst->getUse()[0], inst->getUse()[0]);
        // 这里直接相加
        auto value1 = static_cast<ConstantSymbolEntry *>(inst->getUse()[1]->getEntry())->getValue();
        auto value2 = static_cast<ConstantSymbolEntry *>(nextInst->getUse()[1]->getEntry())->getValue();
        bool floatV = static_cast<ConstantSymbolEntry *>(nextInst->getUse()[1]->getEntry())->getType()->isFloat();
        nextInst->replaceUse(nextInst->getUse()[1], new Operand(new ConstantSymbolEntry(floatV ? TypeSystem::floatType : TypeSystem::intType, floatV ? (float)value1 + (float)value2 : value1 + value2)));
        auto bb = inst->getParent();
        bb->remove(inst);
        return nextInst->getPrev();
    };
    for (auto bb = func->begin(); bb != func->end(); bb++)
    {
        for (auto inst = (*bb)->begin(); inst != (*bb)->end(); inst = inst->getNext())
        {
            if (case1(inst))
                inst = solveCase1(inst);
        }
    }
}

void IRPeepHole::pass()
{
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        subPass(*func);
    }
}
