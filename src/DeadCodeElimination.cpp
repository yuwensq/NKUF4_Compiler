#include "DeadCodeElimination.h"
#include <vector>
#include "debug.h"
using namespace std;

void DeadCodeElimination::pass() 
{
    static int round = 0;
    round++;
    Log("死代码删除开始，round%d\n", round);
    for(auto func=unit->begin();func!=unit->end();func++)
    {
        bool again=true;
        //删除没有前驱的块
        adjustBlock(*func);
        while (again) {
            //1:初始化
            Log("死代码删除:initalize\n");
            initalize(*func);
            //2:标记
            Log("死代码删除:mark\n");
            mark(*func);
            //3:移除
            Log("死代码删除:remove\n");
            again = remove(*func);
            //4:删除没有前驱的块
            Log("死代码删除:adjustBlock\n");
            adjustBlock(*func);
        }       
    }
    Log("死代码删除结束，round%d\n", round);
}

//删除没有前驱的块
void DeadCodeElimination::adjustBlock(Function* func) 
{
    bool again = true;
    while (again)
    {
        again = false;
        vector<BasicBlock*> temp;
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

//一次遍历，完成初步的关键指令标记
void DeadCodeElimination::initalize(Function* func) 
{
    worklist.clear();
    //遍历函数的每一个基本块，完成初始化标记操作
    for (auto it = func->begin(); it != func->end(); it++) 
    {
        //将基本块的mark标记置为false
        (*it)->unsetMark();
        //将基本块其中所有的mark标记置为false（但head应该是没有）
        (*it)->cleanAllMark();
        //遍历指令，执行某一些条件判断，标记其中的一些指令以及基本块，压入worklist
        for (auto it1 = (*it)->begin(); it1 != (*it)->end(); it1 = it1->getNext()) 
        {
            if (it1->isCritical()) {
                //如果是关键指令，就标记它及它所在的基本块
                it1->setMark();
                it1->getParent()->setMark();
                worklist.push_back(it1);
            }
        }
    }
}

void DeadCodeElimination::mark(Function* func) 
{
    //计算反向支配边界
    func->computeRDF();
    
    while(!worklist.empty())
    {
        //每次取出一条已标记指令，why back ? not top?
        auto ins = worklist.back();
        worklist.pop_back();
        //这条已标记指令的所有use操作数的所有定义语句都会被标记
        auto uses = ins->getUse();
        for (auto it : uses) {
            auto def = it->getDef();
            if (def && !def->getMark()) {
                def->setMark();
                def->getParent()->setMark();
                worklist.push_back(def);
            }
        }
        //如果当前这条被标记的指令的存储结果，它被条件/无条件跳转语句所使用，那么就将这一跳转语句标记
        auto def = ins->getDef();
        if (def) {
            for (auto use = def->use_begin(); use != def->use_end(); use++) {
                if (!(*use)->getMark() && ((*use)->isUncond() || (*use)->isCond())) {
                    (*use)->setMark();
                    (*use)->getParent()->setMark();
                    worklist.push_back(*use);
                }
            }
        }
        //当前指令所在基本块的RDF集合基本块，标记其最后一条指令
        auto block = ins->getParent();
        //domFrontier这表示RDF
        for (auto b : block->domFrontier) {
            Instruction* in = b->rbegin();
            if (!in->getMark() && (in->isCond() || in->isUncond())) {
                in->setMark();
                in->getParent()->setMark();
                worklist.push_back(in);
            }
        }
        // 增加对于phi前驱的block的保留
        for (auto in = block->begin(); in != block->end(); in = in->getNext()) {
            if (!in->isPhi())
                continue;
            auto phi = (PhiInstruction*)in;
            for (auto it : phi->getSrcs()) {
                Instruction* in = it.first->rbegin();
                if (!in->getMark() && (in->isCond() || in->isUncond())) {
                    in->setMark();
                    in->getParent()->setMark();
                    worklist.push_back(in);
                }
            }
        }
    }
}

//移除无用指令
bool DeadCodeElimination::remove(Function* func) {
    vector<Instruction*> temp;
    bool ret = false;
    for (auto& block : func->getBlockList()) {
        for (auto it = block->begin(); it != block->end(); it = it->getNext()) {
            //讨论那些没有被标记的指令，看是不是要删除
            if (!it->getMark()) {
                //未被标记的返回语句->有返回值，但是调用这个函数的所有call指令都不利用这个函数的返回值
                //改写：返回0->这是有必要的吗？
                //考虑一种情况 return expr（这个expr非常复杂，那么在这种情况下，我们不会标记这个return，expr的计算也就不会被标记
                //这些计算会被抹去，是有其合理性的
                if (it->isRet()) {
                    auto zero = new Operand(
                        new ConstantSymbolEntry(TypeSystem::intType, 0));
                    it->replaceUse(it->getUse()[0], zero);
                    continue;
                }
                //没有被标记的call指令，给一些处理
                //考虑是critical==0，也就是不必要的那些call指令
                //这边主要的工作是移除removePred标志
                if (it->isCall()) {
                    if (it->isCritical())
                        continue;
                    else {
                        IdentifierSymbolEntry* funcSE = (IdentifierSymbolEntry*)(((CallInstruction*)it)->getFunc());
                        if (!funcSE->isSysy() && funcSE->getName() != "llvm.memset.p0i8.i32") {
                            auto func1 = funcSE->getFunction();
                            func1->removeCallPred(it);
                        }
                    }
                }
                //条件跳转，经过上述处理的call指令，其他指令会被压入
                //未被标记的无条件跳转并不会去删除
                if (!it->isUncond())
                    temp.push_back(it);
                //处理条件跳转，
                if (it->isCond()) {
                    //获取到离它最近的被标记的那个后支配节点去，getMarkBranch后面再过来看看它咋写的
                    BasicBlock* b = func->getMarkBranch(block);
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
    for (auto i : temp) {
        i->getParent()->remove(i);
    }
    return ret;
}