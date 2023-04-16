#include "DeadCodeElimination.h"
#include <vector>

using namespace std;

void DeadCodeElimination::pass() 
{
    for(auto func=unit->begin();func!=unit->end();func++)
    {
        bool again=true;
        //删除没有前驱的块
        adjustBlock(*func);
        while (again) {
            //1:初始化
            initalize(*func);
            //2:标记
            mark(*func);
            //3:移除
            //again = remove(*func);
            //4:删除没有前驱的块
            adjustBlock(*func);
        }       
    }
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