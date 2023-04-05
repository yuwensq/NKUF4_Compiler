#include "PureFunctionAnalyser.h"
#include "debug.h"
#include <queue>

// #define PUREFUNCDEBUG

void PureFunctionAnalyser::analyseCallRelation()
{
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        for (auto bb = (*func)->begin(); bb != (*func)->end(); bb++)
        {
            auto inst = (*bb)->begin();
            while (inst != (*bb)->end())
            {
                if (inst->isCall())
                {
                    auto calleeFunc = static_cast<CallInstruction *>(inst)->getFunc();
                    if (static_cast<IdentifierSymbolEntry *>(calleeFunc)->isSysy())
                    {
                        inst = inst->getNext();
                        continue;
                    }
                    auto calledFunc = unit->se2Func(calleeFunc);
                    callee[*func].insert(calledFunc);
                    caller[calledFunc].insert(*func);
                }
                inst = inst->getNext();
            }
        }
    }
#ifdef PUREFUNCDEBUG
    for (auto pa : callee)
    {
        auto callerName = pa.first->getSymPtr()->toStr();
        Log("%s", callerName.c_str());
        for (auto callee : pa.second)
        {
            auto calleeName = callee->getSymPtr()->toStr();
            Log("\t%s", calleeName.c_str());
        }
    }
#endif
}

bool PureFunctionAnalyser::analyseFuncByStoreAndSysy(Function *)
{
    TODO();
}

void PureFunctionAnalyser::analyseFunc()
{
    analyseCallRelation();
    // unPureList记录每个不纯的函数，并传播不纯的关系和对全局变量的修改
    std::queue<Function *> unPureList;
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        if (!analyseFuncByStoreAndSysy(*func))
        {
            unPureList.push(*func);
        }
    }
    while (!unPureList.empty())
    {
        auto func = unPureList.front();
        unPureList.pop();
        // 该函数的每个caller也都不纯了，对他们的状态进行更新并入队列
        TODO();
    }
}

PureFunctionAnalyser::PureFunctionAnalyser(Unit *unit)
{
    this->unit = unit;
    analyseFunc();
}

bool PureFunctionAnalyser::isPure(Function *func)
{
    if (funcIsPure.find(func) != funcIsPure.end())
        return funcIsPure[func];
    return false;
}
