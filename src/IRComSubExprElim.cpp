#include "IRComSubExprElim.h"
#include <queue>

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
        Assert(loadSrc != nullptr, "啊嘞");
        // 如果是函数参数，这条load就先留着吧，如果改了可能r寄存器被覆盖
        if (static_cast<TemporarySymbolEntry *>(loadSrc->getEntry())->isParam())
            continue;
        // if (loadSrc == nullptr)
        // {
        //     loadInst->output();
        //     continue;
        // }
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
        while (!localCSE(*func) || !globalCSE(*func))
            ;
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
            if (res && res->isAlloc())
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
            {
                // return true;
                // 这里，如果load和store同一个局部数组，我们可以看一看他们的offs相同不，如果不相同，照样注销不了
                auto gepLoad = loadInst->getUse()[0]->getDef();
                auto gepStore = preInst->getUse()[0]->getDef();
                if (!(gepLoad->isGep() && gepStore->isGep()))
                    return true;
                Assert(gepLoad->isGep() && gepStore->isGep(), "不是gep呀");
                auto offs1 = static_cast<GepInstruction *>(gepLoad)->getUse();
                auto offs2 = static_cast<GepInstruction *>(gepStore)->getUse();
                if (offs1.size() != offs2.size())
                    return true;
                bool res = true;
                for (int i = 1; i < offs1.size(); i++)
                {
                    // 判断不出来，默认注销
                    if (!(offs1[i]->getEntry()->isConstant() && offs2[i]->getEntry()->isConstant()))
                        return true;
                    int v1 = static_cast<ConstantSymbolEntry *>(offs1[i]->getEntry())->getValue();
                    int v2 = static_cast<ConstantSymbolEntry *>(offs2[i]->getEntry())->getValue();
                    if (v1 != v2)
                        res = false;
                }
                return res;
            }
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
        {
            return nullptr;
        }
        if (isSameExpr(preInst, inst))
            return preInst;
    }
    return nullptr;
}

bool IRComSubExprElim::localCSE(Function *func)
{
    // return true;
    static int round = 0;
    round++;
    Log("局部子表达式删除开始，round%d", round);
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
    Log("局部子表达式删除结束，round%d", round);
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
                // Log("  %d", exprVec.size());
                // 这里这样写是不是有点慢呀，可不可以搞一个vector存所有的load表达式，后边
                // 如果超时了，可以优化一下这里
                for (auto i = 0; i < exprVec.size(); i++)
                {
                    if (!exprVec[i].inst->isLoad())
                        continue;
                    if (invalidate(inst, exprVec[i].inst)) {
                        killBB[*bb].insert(i);
                        if ((*bb)->getNo() == 36)
                            Log("dawang %d", i);
                    }
                }
            }
            if (inst->getDef() != nullptr)
            {
                // if (!skip(inst))
                // {
                //     int ind = ins2Expr[inst];
                //     killBB[*bb].erase(ind);
                // }
                // 这两种写法是不是等价呀，不过上边这种更快
                for (auto useInst : inst->getDef()->getUse())
                {
                    if (!skip(useInst))
                        killBB[*bb].insert(ins2Expr[useInst]);
                }
                // for (auto index = 0; index < exprVec.size(); index++)
                // {
                //     auto useOps = exprVec[index].inst->getUse();
                //     auto pos = find(useOps.begin(), useOps.end(), inst->getDef());
                //     if (pos != useOps.end())
                //         killBB[*bb].insert(index);
                // }
            }
        }
    }
    Log("fin genkill");
}

void IRComSubExprElim::intersection(std::set<int> &a, std::set<int> &b, std::set<int> &out)
{
    out.clear();
    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), inserter(out, out.begin()));
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
    outBB[entry] = genBB[entry];
    std::set<BasicBlock *> workList;
    for (auto bb = func->begin() + 1; bb != func->end(); bb++)
    {
        outBB[*bb] = U; // 这里直接用等号不会出错吧
        workList.insert(*bb);
    }
    // int sum = 0;
    while (!workList.empty())
    {
        auto bb = *workList.begin();
        workList.erase(workList.begin());
        // 先计算in
        std::set<int> in[2];
        if (bb->getNumOfPred() > 0)
            in[0] = outBB[*bb->pred_begin()];
        auto it = bb->pred_begin() + 1;
        auto overPos = bb->pred_end();
        int turn = 1;
        for (it; it != overPos; it++)
        {
            intersection(outBB[*it], in[turn ^ 1], in[turn]);
            turn ^= 1;
        }
        inBB[bb] = in[turn ^ 1];
        // 再计算out
        std::set<int> midDif;
        std::set<int> out;
        std::set_difference(inBB[bb].begin(), inBB[bb].end(), killBB[bb].begin(), killBB[bb].end(), inserter(midDif, midDif.begin()));
        std::set_union(genBB[bb].begin(), genBB[bb].end(), midDif.begin(), midDif.end(), inserter(out, out.begin()));
        if (out != outBB[bb])
        {
            outBB[bb] = out;
            for (auto succ = bb->succ_begin(); succ != bb->succ_end(); succ++)
                workList.insert(*succ);
        }
    }
    Log("fin inout");
}

bool IRComSubExprElim::removeGlobalCSE(Function *func)
{
    bool result = true;
    bool outChanged = true;
    while (outChanged)
    {
        outChanged = false;
        for (auto bb = func->begin(); bb != func->end(); bb++)
        {
            std::set<std::pair<int, Operand *>> newOutBBOp;
            for (auto &outExpr : outBB[*bb])
            {
                // 如果In无，则用新的，如果In有，且kill，则用新的
                if (inBB[*bb].find(outExpr) == inBB[*bb].end() || killBB[*bb].find(outExpr) != killBB[*bb].end())
                {
                    newOutBBOp.insert(std::make_pair(outExpr, expr2Op[*bb][outExpr]));
                }
            }
            // 如果一个基本块有多个前驱，就算他In里面有许多表达式，我们也不要了，因为要生成phi指令
            // 这里，只在只有一个前驱基本块的情况下继承
            if ((*bb)->pred_end() - (*bb)->pred_begin() == 1)
            {
                auto preBB = *(*bb)->pred_begin();
                for (auto &inExpr : outBBOp[preBB])
                {
                    if (killBB[*bb].find(inExpr.first) == killBB[*bb].end())
                    {
                        newOutBBOp.insert(std::make_pair(inExpr.first, inExpr.second));
                    }
                }
            }
            if (newOutBBOp != outBBOp[*bb])
            {
                outChanged = true;
                outBBOp[*bb] = newOutBBOp;
            }
        }
    }
#ifdef IRCSEDEBUG
    for (auto bb = func->begin(); bb != func->end(); bb++)
    {
        Log("B%d\n", (*bb)->getNo());
        for (auto &pa : outBBOp[*bb])
        {
            Log("    %d %s", pa.first, pa.second->toStr().c_str());
        }
    }
#endif
    for (auto bb = func->begin(); bb != func->end(); bb++)
    {
        bool onlyOnePred = ((*bb)->pred_end() - (*bb)->pred_begin() == 1);
        // 多个前驱的话还要加phi指令，不要了
        if (!onlyOnePred)
            continue;
        auto preBB = *(*bb)->pred_begin();
        std::map<int, Operand *> preBBOutOp;
        for (auto &pa : outBBOp[preBB])
        {
            preBBOutOp[pa.first] = pa.second;
        }
        for (auto inst = (*bb)->begin(); inst != (*bb)->end(); inst = inst->getNext())
        {
            if (skip(inst))
                continue;
            bool inInst = (preBBOutOp.find(ins2Expr[inst]) != preBBOutOp.end());
            if (!inInst || (inst->isLoad() && isKilled(inst)))
                continue;
            // 找到了，可以删除
            result = false;
            std::vector<Instruction *> uses(inst->getDef()->getUse());
            for (auto useInst : uses)
            {
                useInst->replaceUse(inst->getDef(), preBBOutOp[ins2Expr[inst]]);
            }
            auto preInst = inst->getPrev();
            (*bb)->remove(inst);
            inst = preInst;
        }
    }
    // for (auto bb = func->begin(); bb != func->end(); bb++)
    // {
    //     for (auto inst = (*bb)->begin(); inst != (*bb)->end(); inst = inst->getNext())
    //     {
    //         if (skip(inst))
    //             continue;
    //         int index = ins2Expr[inst];
    //         Expr &expr = exprVec[index];
    //         // 如果一个基本块gen了一个表达式，且in里没有这个表达式或kill了这个表达式，
    //         // 对应的指令就是这个表达式的源头定值
    //         bool genInst = genBB[*bb].find(index) != genBB[*bb].end();
    //         bool notInInst = inBB[*bb].find(index) == inBB[*bb].end();
    //         bool killInst = killBB[*bb].find(index) != killBB[*bb].end();
    //         if (genInst && (notInInst || killInst))
    //             expr.srcs.insert(inst);
    //     }
    // }
    // for (auto bb = func->begin(); bb != func->end(); bb++)
    // {
    //     for (auto inst = (*bb)->begin(); inst != (*bb)->end(); inst = inst->getNext())
    //     {
    //         if (skip(inst))
    //             continue;
    //         if (inBB[*bb].count(ins2Expr[inst]) != 0)
    //         {
    //             if (inst->isLoad() && isKilled(inst))
    //                 continue;
    //         }
    //     }
    // }
    return result;
}

bool IRComSubExprElim::globalCSE(Function *func)
{
    static int round = 0;
    round++;
    Log("全局子表达式删除开始，round%d", round);
    exprVec.clear();
    ins2Expr.clear();
    expr2Op.clear();
    genBB.clear();
    killBB.clear();
    inBB.clear();
    outBB.clear();
    inBBOp.clear();
    outBBOp.clear();
    bool result = true;
    calGenAndKill(func);
    calInAndOut(func);
    result = removeGlobalCSE(func);
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
    Log("全局子表达式删除结束，round%d", round);
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
    ins2Expr.clear();
    expr2Op.clear();
    inBBOp.clear();
    outBBOp.clear();
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
