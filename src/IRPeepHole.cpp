#include "IRPeepHole.h"
#include "Type.h"
#include "debug.h"

const int INTMAX = 2147483647;

extern FILE *yyout;

void IRPeepHole::subPass(Function *func)
{
    auto isBinaryConst = [&](Instruction *inst, int opCode)
    {
        bool cond1 = (inst->isBinary() && inst->getOpCode() == opCode && inst->getUse()[1]->getEntry()->isConstant());
        bool cond2 = (inst->isBinary() && inst->getOpCode() == opCode && inst->getUse()[0]->getEntry()->isConstant());
        Operand *useOp = nullptr;
        Operand *constOp = nullptr;
        if (cond1 && !cond2)
        {
            useOp = inst->getUse()[0];
            constOp = inst->getUse()[1];
        }
        else if (cond2 && !cond1)
        {
            useOp = inst->getUse()[1];
            constOp = inst->getUse()[0];
        }
        return std::make_pair(useOp, constOp);
    };
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
    auto case4 = [&](Instruction *inst)
    {
        bool cond1 = (inst->isBinary() && inst->getOpCode() == BinaryInstruction::ADD && inst->getUse()[1]->getEntry()->isConstant() && static_cast<ConstantSymbolEntry *>(inst->getUse()[1]->getEntry())->getValue() == 0);
        bool cond2 = (inst->isBinary() && inst->getOpCode() == BinaryInstruction::ADD && inst->getUse()[0]->getEntry()->isConstant() && static_cast<ConstantSymbolEntry *>(inst->getUse()[0]->getEntry())->getValue() == 0);
        bool cond3 = (inst->getParent() != inst->getParent()->getParent()->getEntry());
        return (cond1 || cond2) && cond3;
    };
    auto solveCase4 = [&](Instruction *inst)
    {
        auto prev = inst->getPrev();
        bool cond1 = (inst->isBinary() && inst->getOpCode() == BinaryInstruction::ADD && inst->getUse()[1]->getEntry()->isConstant() && static_cast<ConstantSymbolEntry *>(inst->getUse()[1]->getEntry())->getValue() == 0);
        std::vector<Instruction *> uses(inst->getDef()->getUse());
        auto replaceUse = cond1 ? inst->getUse()[0] : inst->getUse()[1];
        replaceUse->removeUse(inst);
        for (auto use : uses)
            use->replaceUse(inst->getDef(), replaceUse);
        inst->getParent()->remove(inst);
        return prev;
    };
    auto case5 = [&](Instruction *inst)
    {
        auto [useOp, constOp] = isBinaryConst(inst, BinaryInstruction::ADD);
        if (useOp == nullptr || useOp->getDef() == nullptr)
            return false;
        if (useOp->getDef()->getUse().size() != 1)
            return false;
        auto [useOp2, constOp2] = isBinaryConst(useOp->getDef(), BinaryInstruction::ADD);
        if (useOp2 == nullptr)
            return false;
        if (useOp2->getDef()->getParent() == func->getEntry())
            return false;
        return true;
    };
    auto solveCase5 = [&](Instruction *inst)
    {
        auto [useOp, constOp] = isBinaryConst(inst, BinaryInstruction::ADD);
        auto [useOp2, constOp2] = isBinaryConst(useOp->getDef(), BinaryInstruction::ADD);
        useOp->removeUse(inst);
        inst->replaceUse(useOp, useOp2);
        // 这里直接相加
        auto value1 = static_cast<ConstantSymbolEntry *>(constOp->getEntry())->getValue();
        auto value2 = static_cast<ConstantSymbolEntry *>(constOp2->getEntry())->getValue();
        bool floatV = static_cast<ConstantSymbolEntry *>(useOp2->getEntry())->getType()->isFloat();
        inst->replaceUse(constOp, new Operand(new ConstantSymbolEntry(floatV ? TypeSystem::floatType : TypeSystem::intType, floatV ? (float)value1 + (float)value2 : value1 + value2)));
        auto bb = useOp->getDef()->getParent();
        bb->remove(useOp->getDef());
        return inst->getPrev();
    };
    auto case6 = [&](Instruction *inst)
    {
        auto [mulUseOp, mulConstOp] = isBinaryConst(inst, BinaryInstruction::MUL);
        if (mulUseOp == nullptr || mulConstOp == nullptr || inst->getDef() == nullptr || inst->getDef()->getUse().size() != 1)
            return false;
        auto [divUseOp, divConstOp] = isBinaryConst(inst->getDef()->getUse()[0], BinaryInstruction::DIV);
        if (divUseOp == nullptr || divConstOp == nullptr)
            return false;
        if (!mulConstOp->getType()->isInt())
            return false;
        if (int(static_cast<ConstantSymbolEntry *>(mulConstOp->getEntry())->getValue()) % int(static_cast<ConstantSymbolEntry *>(divConstOp->getEntry())->getValue()) == 0)
            return true;
        return false;
    };
    auto solveCase6 = [&](Instruction *inst)
    {
        auto prevInst = inst->getPrev();
        auto divInst = inst->getDef()->getUse()[0];
        auto mulBB = inst->getParent();
        auto divBB = divInst->getParent();
        auto [mulUseOp, mulConstOp] = isBinaryConst(inst, BinaryInstruction::MUL);
        auto [divUseOp, divConstOp] = isBinaryConst(divInst, BinaryInstruction::DIV);
        auto mulConstValue = int(static_cast<ConstantSymbolEntry *>(mulConstOp->getEntry())->getValue());
        auto divConstValue = int(static_cast<ConstantSymbolEntry *>(divConstOp->getEntry())->getValue());
        divUseOp->removeUse(divInst);
        divConstOp->removeUse(divInst);
        inst->setDef(divInst->getDef());
        inst->replaceUse(mulConstOp, new Operand(new ConstantSymbolEntry(mulConstOp->getType(), mulConstValue / divConstValue)));
        mulBB->remove(inst);
        divBB->insertBefore(inst, divInst);
        divBB->remove(divInst);
        return prevInst;
    };
    auto case7 = [&](Instruction *inst)
    {
        auto [mulUseOp, mulConstOp] = isBinaryConst(inst, BinaryInstruction::MUL);
        if (mulUseOp == nullptr || mulConstOp == nullptr || inst->getDef() == nullptr || inst->getDef()->getUse().size() != 1)
            return false;
        auto addInst = inst->getDef()->getUse()[0];
        if (addInst->isBinary() && addInst->getOpCode() == BinaryInstruction::ADD)
        {
            Operand *baseOp = nullptr;
            if (addInst->getUse()[0] == inst->getDef())
                baseOp = addInst->getUse()[1];
            else if (addInst->getUse()[1] == inst->getDef())
                baseOp = addInst->getUse()[0];
            if (baseOp == nullptr || baseOp != mulUseOp || !mulConstOp->getType()->isInt() || static_cast<ConstantSymbolEntry *>(mulConstOp->getEntry())->getValue() >= INTMAX)
                return false;
            else
                return true;
        }
        return false;
    };
    auto solveCase7 = [&](Instruction *inst)
    {
        auto prevInst = inst->getPrev();
        auto addInst = inst->getDef()->getUse()[0];
        auto mulBB = inst->getParent();
        auto addBB = addInst->getParent();
        auto [mulUseOp, mulConstOp] = isBinaryConst(inst, BinaryInstruction::MUL);
        Operand *baseOp = nullptr;
        Operand *passOp = inst->getDef();
        if (addInst->getUse()[0] == passOp)
            baseOp = addInst->getUse()[1];
        else
            baseOp = addInst->getUse()[0];
        auto mulConstValue = int(static_cast<ConstantSymbolEntry *>(mulConstOp->getEntry())->getValue());
        passOp->removeUse(addInst);
        baseOp->removeUse(addInst);
        inst->setDef(addInst->getDef());
        inst->replaceUse(mulConstOp, new Operand(new ConstantSymbolEntry(mulConstOp->getType(), mulConstValue + 1)));
        mulBB->remove(inst);
        addBB->insertBefore(inst, addInst);
        addBB->remove(addInst);
        return prevInst;
    };
    auto case8 = [&](Instruction *inst)
    {
        auto [mulUseOp, mulConstOp] = isBinaryConst(inst, BinaryInstruction::MUL);
        if (mulUseOp == nullptr || mulConstOp == nullptr || !mulConstOp->getType()->isInt() || static_cast<ConstantSymbolEntry *>(mulConstOp->getEntry())->getValue() != 1)
            return false;
        return true;
    };
    auto solveCase8 = [&](Instruction *inst)
    {
        auto prev = inst->getPrev();
        std::vector<Instruction *> uses(inst->getDef()->getUse());
        auto [mulUseOp, mulConstOp] = isBinaryConst(inst, BinaryInstruction::MUL);
        mulConstOp->removeUse(inst);
        mulUseOp->removeUse(inst);
        for (auto use : uses)
            use->replaceUse(inst->getDef(), mulUseOp);
        inst->getParent()->remove(inst);
        return prev;
    };
    auto case9 = [&](Instruction *inst)
    {
        auto [addUseOp, addConstOp] = isBinaryConst(inst, BinaryInstruction::ADD);
        if (addUseOp == nullptr || addConstOp == nullptr || inst->getDef() == nullptr || inst->getDef()->getUse().size() != 1)
            return false;
        auto [subUseOp, subConstOp] = isBinaryConst(inst->getDef()->getUse()[0], BinaryInstruction::SUB);
        if (subUseOp == nullptr || subConstOp == nullptr || subConstOp != inst->getDef()->getUse()[0]->getUse()[1])
            return false;
        if (!addConstOp->getType()->isInt())
            return false;
        return true;
    };
    auto solveCase9 = [&](Instruction *inst)
    {
        auto prevInst = inst->getPrev();
        auto subInst = inst->getDef()->getUse()[0];
        auto addBB = inst->getParent();
        auto subBB = subInst->getParent();
        auto [addUseOp, addConstOp] = isBinaryConst(inst, BinaryInstruction::ADD);
        auto [subUseOp, subConstOp] = isBinaryConst(subInst, BinaryInstruction::SUB);
        auto addConstValue = int(static_cast<ConstantSymbolEntry *>(addConstOp->getEntry())->getValue());
        auto subConstValue = int(static_cast<ConstantSymbolEntry *>(subConstOp->getEntry())->getValue());
        if (addConstValue >= subConstValue)
        {
            subUseOp->removeUse(subInst);
            subConstOp->removeUse(subInst);
            inst->setDef(subInst->getDef());
            inst->replaceUse(addConstOp, new Operand(new ConstantSymbolEntry(addConstOp->getType(), addConstValue - subConstValue)));
            addBB->remove(inst);
            subBB->insertBefore(inst, subInst);
            subBB->remove(subInst);
        }
        else
        {
            addUseOp->removeUse(inst);
            addConstOp->removeUse(inst);
            subInst->replaceUse(subUseOp, addUseOp);
            subInst->replaceUse(subConstOp, new Operand(new ConstantSymbolEntry(addConstOp->getType(), subConstValue - addConstValue)));
            addBB->remove(inst);
        }
        return prevInst;
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
                if (case4(inst))
                {
                    inst = solveCase4(inst);
                    change = true;
                }
                if (case5(inst))
                {
                    inst = solveCase5(inst);
                    change = true;
                }
                if (case6(inst))
                {
                    inst = solveCase6(inst);
                    change = true;
                }
                if (case7(inst))
                {
                    inst = solveCase7(inst);
                    change = true;
                }
                if (case8(inst))
                {
                    inst = solveCase8(inst);
                    change = true;
                }
                if (case9(inst))
                {
                    inst = solveCase9(inst);
                    change = true;
                }
            }
        }
    } while (change);
}

void IRPeepHole::subPassForBlk(Function *)
{
}

void IRPeepHole::pass()
{
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        subPass(*func);
        subPassForBlk(*func);
    }
}

void IRPeepHole::subPass2(Function *func)
{
    auto case1 = [&](BasicBlock *bb)
    {
        return false;
    };
    bool change = false;
    do
    {
        change = false;
        for (auto bb = func->begin(); bb != func->end(); bb++)
        {
            if (case1(*bb))
            {
                change = true;
            }
        }
    } while (change);
}

void IRPeepHole::pass2()
{
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        subPass2(*func);
    }
}
