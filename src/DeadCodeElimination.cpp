#include "DeadCodeElimination.h"
#include <vector>

using namespace std;

void DeadCodeElimination::pass() 
{
    for(auto func=unit->begin();func!=unit->end();func++)
    {
        bool again=true;
        //删除没有前驱的块
        //adjustBlock(*func);
        while (again) {
            //1:初始化
            initalize(*func);
            /*
            //2:标记
            mark(*func);
            //3:移除
            again = remove(*func);
            //4:删除没有前驱的块
            adjustBlock(*func);
            */
        }       
    }
}

//一次遍历，完成初步的关键指令标记
void DeadCodeElimination::initalize(Function* func) 
{
    vector<Instruction*> worklist;
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