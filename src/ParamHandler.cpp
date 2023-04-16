#include "ParamHandler.h"

void ParamHandler::process(Function *func)
{
    auto entry = func->getEntry();
    // 这里，整数零和浮点数零有差别吗，我忘了
    auto intZero = new Operand(new ConstantSymbolEntry(TypeSystem::intType, 0));
    auto floatZero = new Operand(new ConstantSymbolEntry(TypeSystem::floatType, 0));
    int intParamNum = 0;
    int floatParamNum = 0;
    // 前四个整数用r0-r3，前四个浮点数用s0-s15
    for (auto param : func->getParams())
    {
        // 指针的好像是要str
        if (param->getType()->isPtr() || (param->getType()->isInt() && intParamNum >= 4) || (param->getType()->isFloat() && floatParamNum >= 16))
            continue;
        if (intParamNum >= 4 && floatParamNum >= 16)
            break;
        auto baseType = param->getType();
        auto dst = new Operand(new TemporarySymbolEntry(baseType, SymbolTable::getLabel()));
        auto newInst = new BinaryInstruction(BinaryInstruction::ADD, dst, param, (baseType->isInt() ? intZero : floatZero));
        entry->insertFront(newInst, false);
        auto useInsts = param->getUse();
        for (auto useInst : useInsts)
        {
            if (useInst == newInst)
                continue;
            useInst->replaceUse(param, dst);
        }
        if (baseType->isInt())
            intParamNum++;
        else
            floatParamNum++;
    }
}

void ParamHandler::pass()
{
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        process(*func);
    }
}