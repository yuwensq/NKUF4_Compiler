#include "PureFunctionAnalyser.h"
#include "debug.h"
#include <queue>

extern FILE *yyout;
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
                        callSysy[*func] = true;
                        inst = inst->getNext();
                        continue;
                    }
                    auto calledFunc = unit->se2Func(calleeFunc);
                    callee[*func].insert(calledFunc);
                    caller[calledFunc].insert(std::make_pair(*func, inst));
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
            Log("    %s", calleeName.c_str());
        }
    }
#endif
}

/***
 * 函数返回false，表示源头是全局或者函数参数
 */
bool PureFunctionAnalyser::srcIsLocal(Operand *op, std::string &name)
{
    bool res = true;
    name = "";
    if (op->getEntry()->isVariable())
    {
        // 本身就是一个变量
        // store i32 1, i32* @b, align 4
        auto se = static_cast<IdentifierSymbolEntry *>(op->getEntry());
        name = se->getName();
        res = false;
    }
    else
    {
        // store i32 %t4, i32* %t8, align 4
        // or
        // %t6 = call i32 @getarray(i32* %t5)
        while (dynamic_cast<GepInstruction *>(op->getDef()) != nullptr)
        {
            op = op->getDef()->getUse()[0];
        }
        // 如果是操作了全局数组，就是这种情况
        if (op->getEntry()->isVariable())
        {
            auto se = static_cast<IdentifierSymbolEntry *>(op->getEntry());
            name = se->getName();
            res = false;
        }
        // 这种是操作了函数参数
        else if (dynamic_cast<AllocaInstruction *>(op->getDef()) == nullptr)
        {
            res = false;
        }
    }
    return res;
}

int PureFunctionAnalyser::getArgNumber(Operand *op)
{
    auto func = op->getDef()->getParent()->getParent();
    // if (op->getDef() == nullptr)
    // {
    //     fprintf(yyout, "%s\n", op->toStr().c_str());
    //     fflush(yyout);
    // }
    // op->getDef()->output();
    // fflush(yyout);
    // Log("x");
    // assert(op->getDef()->isGep());
    while (op->getDef() != nullptr && op->getDef()->isGep())
    {
        op = op->getDef()->getUse()[0];
    }
    Assert(op->getDef()->isLoad() || op->getDef()->isBitcast(), "???");
    Operand *arg = nullptr;
    if (op->getDef()->isLoad())
    {
        auto arg_addr = op->getDef()->getUse()[0];
        Assert(arg_addr->getUse()[0]->isStore(), "???");
        arg = arg_addr->getUse()[0]->getUse()[1];
    }
    else if (op->getDef()->isBitcast())
        arg = op->getDef()->getUse()[0];
    return func->getParamNumber(arg);
}

bool PureFunctionAnalyser::analyseFuncWithoutCallee(Function *func)
{
    bool isPure = true;
    std::string name = "";
    for (auto bb = func->begin(); bb != func->end(); bb++)
    {
        for (auto inst = (*bb)->begin(); inst != (*bb)->end(); inst = inst->getNext())
        {
            bool isLocal = true;
            if (inst->isCall())
            {
                auto se = static_cast<IdentifierSymbolEntry *>(static_cast<CallInstruction *>(inst)->getFunc());
                // memset用考虑吗，不用吧，memset好像只用于局部数组的初始化
                if (se->isSysy() && (se->getName() == "getarray" || se->getName() == "getfarray"))
                {
                    isLocal = srcIsLocal(inst->getOperands()[1], name);
                    if (!isLocal)
                    {
                        if (name.size() > 0)
                            funcChangeGlobalVars[func].insert(name);
                        else
                        {
                            changeArgArray[func] = true;
                            int argNum = getArgNumber(inst->getOperands()[1]);
                            // if (argNum == -1)
                            //     Log("here");
                            changeArgNumber[func].insert(argNum);
                        }
                    }
                }
            }
            else if (inst->isStore())
            {
                isLocal = srcIsLocal(inst->getOperands()[0], name);
                if (!isLocal)
                {
                    if (name.size() > 0)
                        funcChangeGlobalVars[func].insert(name);
                    else
                    {

                        changeArgArray[func] = true;
                        int argNum = getArgNumber(inst->getOperands()[0]);
                        // if (argNum == -1) {
                        //     inst->output();
                        //     fflush(yyout);
                        //     Log("here");
                        // }
                        changeArgNumber[func].insert(argNum);
                    }
                }
            }
            if (!isLocal)
            {
                isPure = false;
            }
        }
    }
    return isPure;
}

void PureFunctionAnalyser::analyseFunc()
{
    // phase1
    analyseCallRelation();
    std::queue<Function *> callSysyFuncs;
    // unPureList记录每个不纯的函数，并传播不纯的关系和对全局变量的修改
    std::queue<Function *> unPureList;
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        funcIsPure[*func] = analyseFuncWithoutCallee(*func);
        if (!funcIsPure[*func])
            unPureList.push(*func);
        if (callSysy[*func])
            callSysyFuncs.push(*func);
    }

#ifdef PUREFUNCDEBUG
    Log("%s", "PureFunction Phase1 over");
    for (auto pa : changeArgNumber)
    {
        auto funcName = pa.first->getSymPtr()->toStr();
        Log("%s", funcName.c_str());
        for (auto number : pa.second)
        {
            Log("    %d", number);
        }
    }
#endif

    // phase2
    std::queue<Function *> workList;
    std::string name = "";
    bool isLocal = true;
    for (auto pa : changeArgArray)
    {
        if (pa.second)
            workList.push(pa.first);
    }
    while (!workList.empty())
    {
        auto func = workList.front();
        workList.pop();
        for (auto callerPa : caller[func])
        {
            bool updateCaller = false;
            auto callerFunc = callerPa.first;
            auto callerInst = callerPa.second;
            for (auto argNum : changeArgNumber[func])
            {
                // Log("%d", argNum);
                auto op = callerInst->getUse()[argNum];
                isLocal = srcIsLocal(op, name);
                if (!isLocal)
                {
                    if (funcIsPure[callerFunc])
                    {
                        funcIsPure[callerFunc] = false;
                        unPureList.push(callerFunc);
                    }
                    if (name.size() > 0)
                        funcChangeGlobalVars[callerFunc].insert(name);
                    else
                    {
                        if (changeArgArray.find(callerFunc) == changeArgArray.end() || !changeArgArray[callerFunc])
                            updateCaller = true;
                        changeArgArray[callerFunc] = true;
                        changeArgNumber[callerFunc].insert(getArgNumber(op));
                    }
                }
            }
            if (updateCaller)
                workList.push(callerFunc);
        }
    }

#ifdef PUREFUNCDEBUG
    Log("%s", "PureFunction Phase2 over");
    for (auto pa : changeArgNumber)
    {
        auto funcName = pa.first->getSymPtr()->toStr();
        Log("%s", funcName.c_str());
        for (auto number : pa.second)
        {
            Log("    %d", number);
        }
    }
#endif

    // phase3
    while (!unPureList.empty())
    {
        auto func = unPureList.front();
        unPureList.pop();
        // 该函数的每个caller也都不纯了，对他们的状态进行更新并入队列
        for (auto callerPa : caller[func])
        {
            auto callerFunc = callerPa.first;
            bool updateCaller = false;
            if (funcIsPure[callerFunc])
                updateCaller = true;
            funcIsPure[callerFunc] = false;
            for (auto globalVar : funcChangeGlobalVars[func])
            {
                if (funcChangeGlobalVars[callerFunc].find(globalVar) == funcChangeGlobalVars[callerFunc].end())
                {
                    funcChangeGlobalVars[callerFunc].insert(globalVar);
                    updateCaller = true;
                }
            }
            if (updateCaller)
                unPureList.push(callerFunc);
        }
    }

    // phase4
    while (!callSysyFuncs.empty())
    {
        auto func = callSysyFuncs.front();
        callSysyFuncs.pop();
        // 该函数的每个caller也都不纯了，对他们的状态进行更新并入队列
        for (auto callerPa : caller[func])
        {
            auto callerFunc = callerPa.first;
            if (!callSysy[callerFunc])
                callSysyFuncs.push(callerFunc);
            callSysy[callerFunc] = true;
        }
    }

#ifdef PUREFUNCDEBUG
    Log("%s", "PureFunction Phase3 over");
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        Log("%s %d %d", static_cast<IdentifierSymbolEntry *>((*func)->getSymPtr())->getName().c_str(), funcIsPure[*func], changeArgArray[*func]);
        for (auto globalVar : funcChangeGlobalVars[*func])
            Log("    %s", globalVar.c_str());
    }
#endif
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

bool PureFunctionAnalyser::isReallyPure(Function *func)
{
    return !callSysy[func] && isPure(func);
}

bool PureFunctionAnalyser::changeAArray(Function *func)
{
    if (changeArgArray.find(func) != changeArgArray.end())
        return changeArgArray[func];
    return false;
}

std::set<std::string> &PureFunctionAnalyser::getStoreGlobalVar(Function *func)
{
    return funcChangeGlobalVars[func];
}

std::set<int> &PureFunctionAnalyser::getChangeArgNum(Function *func)
{
    // TODO: insert return statement here
    return changeArgNumber[func];
}
