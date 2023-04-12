#include "IRComSubExprElim.h"

// #define IRCSEDEBUG

extern FILE *yyout;

IRComSubExprElim::IRComSubExprElim(Unit *unit)
{
    this->unit = unit;
    pfa = new PureFunctionAnalyser(unit);
}

IRComSubExprElim::~IRComSubExprElim()
{
    delete pfa;
}

void IRComSubExprElim::insertLoadAfterStore()
{
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        for (auto bb = (*func)->begin(); bb != (*func)->end(); bb++)
        {
            for (auto inst = (*bb)->begin(); inst != (*bb)->end(); inst = inst->getNext())
            {
                if (inst->isStore())
                {
                    auto loadInst = new LoadInstruction(new Operand(new TemporarySymbolEntry(inst->getUse()[1]->getEntry()->getType(), SymbolTable::getLabel())), inst->getUse()[0], nullptr);
                    (*bb)->insertBefore(loadInst, inst->getNext());
                    addedLoad.push_back(std::make_pair(inst, loadInst));
                    inst = loadInst;
                }
            }
        }
    }
}

void IRComSubExprElim::removeLoadAfterStore()
{
    for (auto pa : addedLoad)
    {
        auto loadInst = pa.second;
        auto loadSrc = pa.first->getUse()[1];
        if (loadSrc == nullptr)
        {
            loadInst->output();
            continue;
        }
        auto allUseInst = std::vector<Instruction *>(loadInst->getDef()->getUse());
        for (auto inst : allUseInst)
        {
            inst->replaceUse(loadInst->getDef(), loadSrc);
        }
        loadInst->getParent()->remove(loadInst);
    }
}

void IRComSubExprElim::doCSE()
{
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        bool result1 = false, result2 = false;
        while (!result1 || !result2)
        {
            result1 = localCSE(*func);
            result2 = globalCSE(*func);
        }
    }
}

Instruction *IRComSubExprElim::getSrc(Operand *op, std::string &name)
{
    std::vector<Instruction *> geps;
    Instruction *res = nullptr;
    name = "";
    if (op->getEntry()->isVariable())
    {
        // 本身就是一个变量
        // store i32 1, i32* @b, align 4
        auto se = static_cast<IdentifierSymbolEntry *>(op->getEntry());
        name = se->getName();
    }
    else
    {
        // store i32 %t4, i32* %t8, align 4
        // or
        // %t6 = call i32 @getarray(i32* %t5)
        while (dynamic_cast<GepInstruction *>(op->getDef()) != nullptr)
        {
            if (gep2Alloc.find(op->getDef()) != gep2Alloc.end())
            {
                res = gep2Alloc[op->getDef()];
                return res;
            }
            geps.push_back(op->getDef());
            op = op->getDef()->getUse()[0];
        }
        // 如果是操作了全局数组，就是这种情况
        if (op->getEntry()->isVariable())
        {
            auto se = static_cast<IdentifierSymbolEntry *>(op->getEntry());
            name = se->getName();
        }
        // 局部变量或者函数参数，返回源头指令
        else
        {
            res = op->getDef();
            if (res->isAlloc())
            {
                for (auto gep : geps)
                    gep2Alloc[gep] = res;
            }
        }
    }
    return res;
}

bool IRComSubExprElim::invalidate(Instruction *preInst, Instruction *loadInst)
{
    Assert(loadInst->isLoad(), "这传的参数不对呀");
    bool preIsCall = false;
    Function *func = nullptr;
    // 上一条指令的源头的名称和定义源的alloc指令
    std::string nameStore = "";
    Instruction *allocInstPre = nullptr;
    // load指令的源头的名称和定义源的alloc指令
    std::string nameLoad = "";
    Instruction *allocInstLoad = nullptr;
    if (preInst->isCall())
    {
        preIsCall = true;
        func = unit->se2Func(static_cast<CallInstruction *>(preInst)->getFunc());
    }
    else if (preInst->isStore())
    {
        allocInstPre = getSrc(preInst->getUse()[0], nameStore);
    }
    else
        return false;
    allocInstLoad = getSrc(loadInst->getUse()[0], nameLoad);
    // load一个全局变量或全局数组
    if (nameLoad.size() > 0)
    {
        // preInst是函数调用
        if (preIsCall)
        {
            // preInst调用的函数改了load对应的全局元素
            if (pfa->getStoreGlobalVar(func).count(nameLoad) != 0)
                return true;
            // load对应的全局数组作为参数传给preInst调用的函数，且这个函数改了这个参数
            for (auto argNum : pfa->getChangeArgNum(func))
            {
                std::string argName = "";
                getSrc(preInst->getUse()[argNum], argName);
                if (argName == nameLoad)
                    return true;
            }
        }
        // 有个store改了load的源头，或者store了一个函数参数数组(由于我们也不知道传入的参数是啥，这里就把load无效化)
        else
        {
            if (nameStore == nameLoad)
                return true;
            else if (!loadInst->getUse()[0]->getEntry()->isVariable() && nameStore == "" && dynamic_cast<AllocaInstruction *>(allocInstPre) == nullptr)
                return true;
        }
        return false;
    }
    else // load一个参数数组或局部数组，局部变量已经被mem2reg搞没了
    {
        // load一个局部数组
        if (dynamic_cast<AllocaInstruction *>(allocInstLoad) != nullptr)
        {
            if (preIsCall)
            {
                for (auto argNum : pfa->getChangeArgNum(func))
                {
                    std::string argName = "";
                    auto srcInst = getSrc(preInst->getUse()[argNum], argName);
                    if (allocInstLoad == srcInst)
                        return true;
                }
            }
            else if (allocInstLoad == allocInstPre)
                return true;
            return false;
        }
        // load一个参数数组
        else
        {
            // 非纯函数
            if (preIsCall)
                return pfa->isPure(func);
            // store全局数组
            if (!preInst->getUse()[0]->getEntry()->isVariable() && nameStore.size() > 0)
                return true;
            // store函数参数数组
            if (nameStore.size() <= 0 && dynamic_cast<AllocaInstruction *>(allocInstPre) == nullptr)
                return true;
            return false;
        }
    }
    return false;
}

bool IRComSubExprElim::isSameExpr(Instruction *inst1, Instruction *inst2)
{
    if (inst1->getType() != inst2->getType() || inst1->getOpCode() != inst2->getOpCode())
        return false;
    auto ops1 = inst1->getUse();
    auto ops2 = inst2->getUse();
    if (ops1.size() != ops2.size())
        return false;
    auto op2 = ops2.begin();
    for (auto op1 : ops1)
    {
        auto se1 = op1->getEntry();
        auto se2 = (*op2)->getEntry();
        if (se1->isConstant() && se2->isConstant())
        {
            if (se1->getType()->isFloat() && se2->getType()->isFloat())
            {
                if (static_cast<float>(static_cast<ConstantSymbolEntry *>(se1)->getValue()) != static_cast<float>(static_cast<ConstantSymbolEntry *>(se2)->getValue()))
                    return false;
            }
            else if (se1->getType()->isInt() && se2->getType()->isInt())
            {
                if (static_cast<int>(static_cast<ConstantSymbolEntry *>(se1)->getValue()) != static_cast<int>(static_cast<ConstantSymbolEntry *>(se2)->getValue()))
                    return false;
            }
            else
                return false;
        }
        else if (op1 != (*op2))
            return false;
        op2++;
    }
    return true;
}

bool IRComSubExprElim::skip(Instruction *inst)
{
    if (inst->isBinaryCal() || inst->isUnaryCal() || inst->isGep() || inst->isLoad())
        return false;
    return true;
}

/**
 * 函数目前实现的逻辑较为简单，就是从后到前挨个遍历指令，看有没有指令和当前的inst
 * 是同一个子表达式，如果后期编译超时，可以考虑换换数据结构什么的
 */
Instruction *IRComSubExprElim::preSameExpr(Instruction *inst)
{
    Instruction *preInst = nullptr;
    auto bb = inst->getParent();
    for (preInst = inst->getPrev(); preInst != bb->end(); preInst = preInst->getPrev())
    {
        if (inst->isLoad() && invalidate(preInst, inst))
            return nullptr;
        // if (isSameExpr(preInst, inst))
        //     return preInst;
    }
    return nullptr;
}

bool IRComSubExprElim::localCSE(Function *func)
{
    bool result = true;
    for (auto bb = func->begin(); bb != func->end(); bb++)
    {
        for (auto inst = (*bb)->begin(); inst != (*bb)->end(); inst = inst->getNext())
        {
            if (skip(inst))
                continue;
            // 这里如果后期编译超时可以优化一下，preSameExpr函数的逻辑
            auto preInst = preSameExpr(inst);
            if (preInst != nullptr)
            {
                // 这里遇到了一个奇奇怪怪的bug，非要这样才可以
                std::vector<Instruction *> uses(inst->getDef()->getUse());
                result = false;
                for (auto useInst : uses)
                {
                    useInst->replaceUse(inst->getDef(), preInst->getDef());
                }
                preInst = inst->getPrev();
                (*bb)->remove(inst);
                inst = preInst;
            }
        }
    }
    return result;
}

/**
 * 这个函数写的也比较简单
 */
bool IRComSubExprElim::isKilled(Instruction *inst)
{
    Instruction *preInst = nullptr;
    auto bb = inst->getParent();
    for (preInst = inst->getPrev(); preInst != bb->end(); preInst = preInst->getPrev())
    {
        if (inst->isLoad() && invalidate(preInst, inst))
            return true;
    }
    return false;
}

void IRComSubExprElim::calGenAndKill(Function *func)
{
    // 参考链接https://eternalsakura13.com/2018/08/08/optimize/
    // 先算gen和找到所有的expr
    Log("start genkill");
    std::vector<int> removeExpr;
    // int sum = 0;
    for (auto bb = func->begin(); bb != func->end(); bb++)
    {
        for (auto inst = (*bb)->begin(); inst != (*bb)->end(); inst = inst->getNext())
        {
            // sum++;
            // Log("%d", sum);
            // 如果遇到了一条store或call，可能把已经gen的load指令给kill了
            if (inst->isStore() || inst->isCall())
            {
                removeExpr.clear();
                for (auto &index : genBB[*bb])
                {
                    if (exprVec[index].inst->isLoad() && invalidate(inst, exprVec[index].inst))
                    {
                        removeExpr.push_back(index);
                    }
                }
                for (auto &index : removeExpr)
                {
                    genBB[*bb].erase(index);
                    expr2Op[*bb].erase(index);
                }
                continue;
            }
            if (skip(inst))
                continue;
            Expr expr(inst);
            auto it = find(exprVec.begin(), exprVec.end(), expr);
            int ind = it - exprVec.begin();
            if (it == exprVec.end())
            {
                exprVec.push_back(expr);
            }
            ins2Expr[inst] = ind;
            // 如果是一个load指令，要看看之后会不会有store或者call指令把这个load指令给kill掉
            // if (inst->isLoad() && isKilled(inst))
            //     continue;
            genBB[*bb].insert(ind);
            expr2Op[*bb][ind] = inst->getDef();
            /*
                一个基本块内不会出现这种 t1 = t2 + t3
                                       t2 = ...
                所以这里，之后gen的表达式不会kill掉已经gen的表达式
            */
            // 从genBB中删除和inst->getDef相关的表达式
            // removeExpr.clear();
            // for (auto &index : genBB[*bb])
            // {
            //     auto useOps = exprVec[index].inst->getUse();
            //     auto pos = find(useOps.begin(), useOps.end(), inst->getDef());
            //     if (pos != useOps.end())
            //         removeExpr.push_back(index);
            // }
            // for (auto &index : removeExpr)
            //     genBB[*bb].erase(index);
        }
    }
    // sum = 0;
    // 接下来算kill
    for (auto bb = func->begin(); bb != func->end(); bb++)
    {
        for (auto inst = (*bb)->begin(); inst != (*bb)->end(); inst = inst->getNext())
        {
            // sum++;
            // Log("%d", sum);
            // 碰到了store或call指令，就把他们能kill的load指令加到killBB中
            if (inst->isStore() || inst->isCall())
            {
                Log("  %d", exprVec.size());
                for (auto i = 0; i < exprVec.size(); i++)
                {
                    if (!exprVec[i].inst->isLoad())
                        continue;
                    if (invalidate(inst, exprVec[i].inst))
                        killBB[*bb].insert(i);
                }
            }
            if (inst->getDef() != nullptr)
            {
                if (!skip(inst))
                {
                    int ind = ins2Expr[inst];
                    killBB[*bb].erase(ind);
                }
                for (auto index = 0; index < exprVec.size(); index++)
                {
                    auto useOps = exprVec[index].inst->getUse();
                    auto pos = find(useOps.begin(), useOps.end(), inst->getDef());
                    if (pos != useOps.end())
                        killBB[*bb].insert(index);
                }
            }
        }
    }
    Log("fin genkill");
}

std::set<int> IRComSubExprElim::intersection(std::set<int> &a, std::set<int> &b)
{
    std::vector<int> res;
    res.resize(std::min(a.size(), b.size()));
    auto lastPos = std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), res.begin());
    return std::set<int>(res.begin(), lastPos);
}

void IRComSubExprElim::calInAndOut(Function *func)
{
    // 准备个U
    Log("start inout");
    std::set<int> U;
    for (int i = 0; i < exprVec.size(); i++)
        U.insert(i);
    auto entry = func->getEntry();
    inBB[entry].clear();
    for (auto bb = func->begin(); bb != func->end(); bb++)
        outBB[*bb] = U; // 这里直接用等号不会出错吧
    bool outChanged = true;
    // int sum = 0;
    while (outChanged)
    {
        // sum++;
        // Log("%d", sum);
        outChanged = false;
        for (auto bb = func->begin(); bb != func->end(); bb++)
        {
            // 先计算in
            if (*bb != entry)
            {
                std::set<int> in = U;
                for (auto preB = (*bb)->pred_begin(); preB != (*bb)->pred_end(); preB++)
                {
                    in = intersection(outBB[*preB], in);
                }
                inBB[*bb] = in;
            }
            // 再计算out
            std::set<int> midDif;
            std::set<int> out;
            std::set_difference(inBB[*bb].begin(), inBB[*bb].end(), killBB[*bb].begin(), killBB[*bb].end(), inserter(midDif, midDif.begin()));
            std::set_union(genBB[*bb].begin(), genBB[*bb].end(), midDif.begin(), midDif.end(), inserter(out, out.begin()));
            if (out != outBB[*bb])
                outChanged = true;
            outBB[*bb] = out;
        }
    }
    Log("fin inout");
}

void IRComSubExprElim::removeGlobalCSE(Function *func)
{
    for (auto bb = func->begin(); bb != func->end(); bb++)
    {
        for (auto inst = (*bb)->begin(); inst != (*bb)->end(); inst = inst->getNext())
        {
            if (skip(inst))
                continue;
            if (inBB[*bb].count(ins2Expr[inst]) != 0) {
                if (inst->isLoad() && isKilled(inst))
                    continue;
                
            }
        }
    }
}

bool IRComSubExprElim::globalCSE(Function *func)
{
    exprVec.clear();
    ins2Expr.clear();
    expr2Op.clear();
    genBB.clear();
    killBB.clear();
    inBB.clear();
    outBB.clear();
    bool result = true;
    calGenAndKill(func);
    calInAndOut(func);
    removeGlobalCSE(func);
#ifdef IRCSEDEBUG
    fprintf(yyout, "all expr\n");
    for (auto index = 0; index < exprVec.size(); index++)
    {
        fprintf(yyout, "%d ", index);
        exprVec[index].inst->output();
    }
    fprintf(yyout, "gen per bb\n");
    for (auto bb = func->begin(); bb != func->end(); bb++)
    {
        fprintf(yyout, "\n%%B%d\n", (*bb)->getNo());
        for (auto index : genBB[*bb])
            fprintf(yyout, "    %d", index);
    }
    fprintf(yyout, "\nkill per bb\n");
    for (auto bb = func->begin(); bb != func->end(); bb++)
    {
        fprintf(yyout, "\n%%B%d\n", (*bb)->getNo());
        for (auto index : killBB[*bb])
            fprintf(yyout, "    %d", index);
    }
    fprintf(yyout, "\nin per bb");
    for (auto bb = func->begin(); bb != func->end(); bb++)
    {
        fprintf(yyout, "\n%%B%d\n", (*bb)->getNo());
        for (auto index : inBB[*bb])
            fprintf(yyout, "    %d", index);
    }
    fprintf(yyout, "\nout per bb\n");
    for (auto bb = func->begin(); bb != func->end(); bb++)
    {
        fprintf(yyout, "\n%%B%d\n", (*bb)->getNo());
        for (auto index : outBB[*bb])
            fprintf(yyout, "    %d", index);
    }
    fprintf(yyout, "\n");
#endif
    return result;
}

void IRComSubExprElim::clearData()
{
    addedLoad.clear();
    gep2Alloc.clear();
    exprVec.clear();
    genBB.clear();
    killBB.clear();
    inBB.clear();
    outBB.clear();
}

void IRComSubExprElim::pass()
{
    Log("公共子表达式删除开始");
    clearData();
    // 这个加load可能会导致变慢
    insertLoadAfterStore();
    doCSE();
    removeLoadAfterStore();
    Log("公共子表达式删除结束\n");
}
