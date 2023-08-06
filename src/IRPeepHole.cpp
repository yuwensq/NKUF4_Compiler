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
    auto case2 = [&](Instruction *inst)
    {
        bool cond1 = inst->isBinary() && inst->getOpCode() == BinaryInstruction::ADD;
        if (!cond1)
            return 0;
        auto bb = inst->getParent();
        int addNum = 1;
        for (auto ins = inst->getNext(); ins != bb->end(); ins = ins->getNext())
        {
            auto prevIns = ins->getPrev();
            if (!(ins->isBinary() && ins->getOpCode() == BinaryInstruction::ADD))
                break;
            if (prevIns->getDef() == ins->getUse()[0] && prevIns->getUse()[1] == ins->getUse()[1] && prevIns->getDef()->usersNum() == 1)
                addNum++;
            else
                break;
        }
        return addNum;
    };
    auto solveCase2 = [&](Instruction *inst)
    {
        auto bb = inst->getParent();
        int addNum = 1;
        auto ins = inst->getNext();
        auto src1 = inst->getUse()[0];
        auto src2 = inst->getUse()[1];
        src1->removeUse(inst);
        for (ins; ins != bb->end(); ins = ins->getNext())
        {
            auto prevIns = ins->getPrev();
            if (!(ins->isBinary() && ins->getOpCode() == BinaryInstruction::ADD))
                break;
            if (prevIns->getDef() == ins->getUse()[0] && prevIns->getUse()[1] == ins->getUse()[1] && prevIns->getDef()->usersNum() == 1)
            {
                src2->removeUse(prevIns);
                bb->remove(prevIns);
                addNum++;
            }
            else
                break;
        }
        assert(addNum >= 5);
        ins = ins->getPrev();
        ins->replaceUse(ins->getUse()[0], src1);
        auto newOp = new Operand(new TemporarySymbolEntry(ins->getDef()->getType(), SymbolTable::getLabel()));
        auto newMul = new BinaryInstruction(BinaryInstruction::MUL, newOp, src2, new Operand(new ConstantSymbolEntry(TypeSystem::intType, addNum)));
        bb->insertBefore(newMul, ins);
        ins->replaceUse(ins->getUse()[1], newOp);
        return ins;
    };
    auto case3 = [&](Instruction *inst)
    {
        auto nextInst = inst->getNext();
        bool cond1 = (inst->isBinary() && inst->getOpCode() == BinaryInstruction::ADD && inst->getUse()[1]->getEntry()->isConstant() && static_cast<ConstantSymbolEntry *>(inst->getUse()[1]->getEntry())->getValue() == 0);
        bool cond2 = (nextInst->isBinary() && nextInst->getOpCode() == BinaryInstruction::ADD);
        bool cond3 = cond1 && cond2 && (nextInst->getUse()[0] == inst->getDef());
        bool cond4 = cond3 && (inst->getDef()->usersNum() == 1);
        return cond4;
    };
    auto solveCase3 = [&](Instruction *inst)
    {
        auto nextInst = inst->getNext();
        inst->getUse()[0]->removeUse(inst);
        nextInst->replaceUse(nextInst->getUse()[0], inst->getUse()[0]);
        // 这里直接相加
        auto bb = inst->getParent();
        bb->remove(inst);
        return nextInst->getPrev();
    };
    bool change = false;
    do
    {
        change = false;
        for (auto bb = func->begin(); bb != func->end(); bb++)
        {
            for (auto inst = (*bb)->begin(); inst != (*bb)->end(); inst = inst->getNext())
            {
                if (case1(inst))
                {
                    inst = solveCase1(inst);
                    change = true;
                }
                if (case2(inst) >= 5)
                {
                    inst = solveCase2(inst);
                    change = true;
                }
                if (case3(inst))
                {
                    inst = solveCase3(inst);
                    change = true;
                }
            }
        }
    } while (change);
}

void IRPeepHole::pass()
{
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        subPass(*func);
    }
}

void IRPeepHole::subPass2(Function *)
{
}

void IRPeepHole::pass2()
{
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        subPass2(*func);
    }
}
