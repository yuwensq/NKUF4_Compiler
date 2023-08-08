#include "DeadCodeElimination.h"
#include <vector>
#include "debug.h"
using namespace std;

void DeadCodeElimination::pass()
{
    static int round = 0;
    round++;
    gepOp.clear();
    gloOp.clear();
    allocOp.clear();
    // Log("死代码删除开始，round%d\n", round);
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        bool again = true;
        // 删除没有前驱的块
        adjustBlock(*func);
        while (again)
        {
            // 1:初始化
            // Log("死代码删除:initalize\n");
            initalize(*func);
            // 2:标记
            // Log("死代码删除:mark\n");
            mark(*func);
            // 3:移除
            // Log("死代码删除:remove\n");
            again = remove(*func);
            // 4:删除没有前驱的块
            // Log("死代码删除:adjustBlock\n");
            adjustBlock(*func);
        }
    }
    // Log("死代码删除结束，round%d\n", round);
}

// 删除没有前驱的块
void DeadCodeElimination::adjustBlock(Function *func)
{
    bool again = true;
    while (again)
    {
        again = false;
        vector<BasicBlock *> temp;
        for (auto block : func->getBlockList())
            if (block->getNumOfPred() == 0 && block != func->getEntry())
                temp.push_back(block);
        if (temp.size())
            again = true;
        for (auto block : temp)
        {
            delete block;
        }
    }
}

// 一次遍历，完成初步的关键指令标记
void DeadCodeElimination::initalize(Function *func)
{
    worklist.clear();
    // 遍历函数的每一个基本块，完成初始化标记操作
    for (auto it = func->begin(); it != func->end(); it++)
    {
        // 将基本块的mark标记置为false
        (*it)->unsetMark();
        // 将基本块其中所有的mark标记置为false
        (*it)->cleanAllMark();
        // 遍历指令，执行某一些条件判断，标记其中的一些指令以及基本块，压入worklist
        for (auto it1 = (*it)->begin(); it1 != (*it)->end(); it1 = it1->getNext())
        {
            // call指令的参数会影响前面store是否保存
            if (it1->isCall())
            {
                addCriticalOp(it1);
            }
            if (it1->isCritical())
            {
                // 如果是关键指令，就标记它及它所在的基本块
                it1->setMark();
                it1->getParent()->setMark();
                worklist.push_back(it1);
            }
        }
    }
}

void DeadCodeElimination::mark(Function *func)
{
    int opNum = 0;
    // 计算反向支配边界
    func->computeRDF();

    while (true)
    {
        // Log("markBasic:begin\n");
        markBasic(func);
        // Log("markBasic:end\n");
        int temp = gepOp.size() + gloOp.size() + allocOp.size();
        if (temp > opNum)
        {
            opNum = temp;
        }
        else
        {
            break;
        }
        // Log("markStore:begin\n");
        markStore(func);
        // Log("markStore:end\n");
    }
    // for(auto t:gepOp) std::cout<<t->toStr()<<std::endl;
}

void DeadCodeElimination::markBasic(Function *func)
{
    while (!worklist.empty())
    {
        // 每次取出一条已标记指令，why back ? not top?
        auto ins = worklist.back();
        worklist.pop_back();
        // 这条已标记指令的所有use操作数的所有定义语句都会被标记
        auto uses = ins->getUse();
        for (auto it : uses)
        {
            auto def = it->getDef();
            if (def && !def->getMark())
            {
                if (def->isLoad())
                {
                    addCriticalOp(def);
                }
                def->setMark();
                def->getParent()->setMark();
                worklist.push_back(def);
            }
        }
        // 如果当前这条被标记的指令的存储结果，它被条件/无条件跳转语句所使用，那么就将这一跳转语句标记
        auto def = ins->getDef();
        if (def)
        {
            for (auto use = def->use_begin(); use != def->use_end(); use++)
            {
                if (!(*use)->getMark() && ((*use)->isUncond() || (*use)->isCond()))
                {
                    (*use)->setMark();
                    (*use)->getParent()->setMark();
                    worklist.push_back(*use);
                }
            }
        }
        // 当前指令所在基本块的RDF集合基本块，标记其最后一条指令
        auto block = ins->getParent();
        // domFrontier这表示RDF
        for (auto b : block->domFrontier)
        {
            Instruction *in = b->rbegin();
            if (!in->getMark() && (in->isCond() || in->isUncond()))
            {
                in->setMark();
                in->getParent()->setMark();
                worklist.push_back(in);
            }
        }
        // 增加对于phi前驱的block的保留
        for (auto in = block->begin(); in != block->end(); in = in->getNext())
        {
            if (!in->isPhi())
                continue;
            auto phi = (PhiInstruction *)in;
            for (auto it : phi->getSrcs())
            {
                Instruction *in = it.first->rbegin();
                if (!in->getMark() && (in->isCond() || in->isUncond()))
                {
                    in->setMark();
                    in->getParent()->setMark();
                    worklist.push_back(in);
                }
            }
        }
    }
}

void DeadCodeElimination::addCriticalOp(Instruction *ins)
{
    if (ins->isLoad())
    {
        // 获取这条load指令的src
        Operand *use = ins->getUse()[0];
        // 如果是全局
        if (use->isGlobal())
        {
            gloOp.insert(use);
        }
        else
        {
            // 如果是数组指针,获取那条数组求地址指令的所有use
            Instruction *def = use->getDef();
            // 前继def是alloc指令
            if (def->isAlloc())
            {
                allocOp.insert(use);
            }
            else
            {
                // 前继def是gep指令
                auto arrUse = def->getUse()[0];
                gepOp.insert(arrUse);
                // 如果arrUse的定义语句仍然是gep，那也要把那个加入进来
                Instruction *def2 = arrUse->getDef();
                if (def2 && def2->isGep())
                {
                    gepOp.insert(def2->getUse()[0]);
                }
            }
        }
    }
    if (ins->isCall())
    {
        // 如果它的参数中有指针，那么我们要将对应的数组首地址压入gepOp，对他的store要保留
        // 同时应考虑前继为alloc
        // 如果参数有全局数组的话，他前面会先load的
        std::vector<Operand *> params = ins->getUse();
        if (!params.empty())
        {
            for (auto param : params)
            {
                if (param->getType()->isPtr())
                {
                    Instruction *defIns = param->getDef();
                    if (defIns->isAlloc())
                    {
                        allocOp.insert(param);
                    }
                    else if (defIns->isGep())
                    {
                        // 这里要注意，有可能是memset函数，i8*参数，前一条bit指令我们不要的
                        gepOp.insert(defIns->getUse()[0]);
                    }
                }
            }
        }
    }
}

void DeadCodeElimination::markStore(Function *func)
{
    // 标记关键的store指令
    for (auto block : func->getBlockList())
    {
        Instruction *ins = block->begin();
        while (ins != block->end())
        {
            if (ins->isStore())
            {
                Operand *dstOP = ins->getUse()[0];
                Operand *srcOp = ins->getUse()[1];
                // 全局的标记
                if (dstOP->isGlobal())
                {
                    for (auto glo : gloOp)
                    {
                        if (dstOP->toStr() == glo->toStr())
                        {
                            ins->setMark();
                            ins->getParent()->setMark();
                            worklist.push_back(ins);
                        }
                    }
                    // if(count(gloOp.begin(),gloOp.end(),dstOP)){
                    //     ins->setMark();
                    //     ins->getParent()->setMark();
                    //     worklist.push_back(ins);
                    // }
                    // alloc或gep的标记
                }
                else
                {
                    Instruction *def = dstOP->getDef(); // def是gep或alloc指令
                    // 我们使用toStr()比较名称是否相同，而不直接比较两个operand*对象
                    // 因为前面优化处理可能让两个不同operand*最后有相同的名称等等
                    if (def->isAlloc())
                    {
                        if (!allocOp.empty())
                        {
                            for (auto allOp : allocOp)
                            {
                                if (allOp->toStr() == dstOP->toStr())
                                {
                                    ins->setMark();
                                    ins->getParent()->setMark();
                                    worklist.push_back(ins);
                                    break;
                                }
                            }
                        }
                        // 考虑函数内联处理后的情况，这个函数中有数组参数，此时这条指令被抹除了，我们需要找到函数内联后
                        // 实参数组与形参数组，二者之间联系的那条指令，就是一条store，将实参地址存入某一个**类型的形参中
                        // 如果这条store它的use[1]是某一个关键ArrUse数组的首地址，它的use[0]就是一个**类型的操作数
                        // 而后面倘若又load了这个**类型的地址到一个新的数组变量中，那么就要把它加入gepOp
                        Instruction *srcDef = srcOp->getDef();
                        // 避免参数的store，所以先行判断srcDef存在
                        if (srcDef && srcDef->isGep())
                        {
                            Operand *op1 = srcDef->getUse()[0];
                            for (auto ArrUse : gepOp)
                            {
                                if (op1->toStr() == ArrUse->toStr())
                                {
                                    // 是关键的
                                    std::vector<Instruction *> loadIns = dstOP->getUse();
                                    for (auto loads : loadIns)
                                    {
                                        if (loads->isLoad())
                                        {
                                            gepOp.insert(loads->getDef());
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    else if (!gepOp.empty())
                    {
                        auto use = def->getUse()[0];
                        Operand *use1 = nullptr; // 父节点
                        Instruction *def2 = use->getDef();
                        if (def2 && def2->isGep())
                        {
                            use1 = def2->getUse()[0];
                        }

                        for (auto ArrUse : gepOp)
                        {
                            if (use->toStr() == ArrUse->toStr() || (use1 && use1->toStr() == ArrUse->toStr()))
                            {
                                ins->setMark();
                                ins->getParent()->setMark();
                                worklist.push_back(ins);
                                break;
                            }
                        }
                    }
                }
            }
            // 补充一下，如%t176 = getelementptr inbounds [6 x i32], [6 x i32]* %t363, i32 0, i32 0
            // 363被标记了，那么如果gep指令，use[0]是363，use[1:]偏移是0，就也把176放进去
            if (ins->isGep())
            {
                std::vector<Operand *> useOp = ins->getUse();
                if (useOp[0]->getType()->isPtr())
                {
                    if (((PointerType *)useOp[0]->getType())->getType()->isArray())
                    {
                        // std::cout<<useOp[0]->toStr()<<std::endl;
                        if (!gepOp.empty())
                        {
                            for (auto ArrUse : gepOp)
                            {
                                if (useOp[0]->toStr() == ArrUse->toStr())
                                {
                                    bool allzero = true;
                                    for (auto i = 1; i < useOp.size(); i++)
                                    {
                                        if (useOp[i]->toStr() != "0")
                                        {
                                            allzero = false;
                                            break;
                                        }
                                    }
                                    if (allzero)
                                    {
                                        gepOp.insert(ins->getDef());
                                        // std::cout<<(ins->getDef())->toStr()<<std::endl;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            ins = ins->getNext();
        }
    }
}

// 移除无用指令
bool DeadCodeElimination::remove(Function *func)
{
    vector<Instruction *> temp;
    bool ret = false;
    for (auto &block : func->getBlockList())
    {
        for (auto it = block->begin(); it != block->end(); it = it->getNext())
        {
            // 讨论那些没有被标记的指令，看是不是要删除
            if (!it->getMark())
            {
                // 未被标记的返回语句->有返回值，但是调用这个函数的所有call指令都不利用这个函数的返回值
                // 考虑一种情况 return expr（这个expr非常复杂，那么在这种情况下，我们不会标记这个return，expr的计算也就不会被标记
                // 这些计算会被抹去，是有其合理性的
                if (it->isRet())
                {
                    auto zero = new Operand(
                        new ConstantSymbolEntry(TypeSystem::intType, 0));
                    it->replaceUse(it->getUse()[0], zero);
                    continue;
                }
                // 没有被标记的call指令，给一些处理
                // 考虑是critical==0，也就是不必要的那些call指令
                // 这边主要的工作是移除removePred标志
                if (it->isCall())
                {
                    if (it->isCritical())
                        continue;
                    else
                    {
                        IdentifierSymbolEntry *funcSE = (IdentifierSymbolEntry *)(((CallInstruction *)it)->getFunc());
                        if (!funcSE->isSysy() && funcSE->getName() != "llvm.memset.p0i8.i32")
                        {
                            auto func1 = funcSE->getFunction();
                            func1->removeCallPred(it);
                        }
                    }
                }
                // 条件跳转，经过上述处理的call指令，其他指令会被压入
                // 未被标记的无条件跳转并不会去删除
                if (!it->isUncond())
                    temp.push_back(it);
                // 处理条件跳转
                if (it->isCond())
                {
                    // 获取到离它最近的被标记的那个后支配节点去，getMarkBranch后面再过来看看它咋写的
                    BasicBlock *b = func->getMarkBranch(block);
                    if (!b)
                        // 这种情况只能是整个函数都没用 所以不处理了
                        return false;
                    new UncondBrInstruction(b, block);
                    block->cleanAllSucc();
                    block->addSucc(b);
                    b->addPred(block);
                }
            }
        }
    }
    if (temp.size())
        ret = true;
    for (auto i : temp)
    {
        // 对于指令的use操作数，这条被删除的指令可能放在这些use操作数的getUse之中，要删除这种关系
        for (auto useOp : i->getUse())
        {
            useOp->removeUse(i);
        }
        i->getParent()->remove(i);
    }
    return ret;
}