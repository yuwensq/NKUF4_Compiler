#include "Global2Local.h"
#include <numeric>
using namespace std;

void Global2Local::pass() {
    recordGlobals();
    // auto iter = unit->begin();
    // while (iter != unit->end())
    //     pass(*iter++);
    // unstoreGlobal2Const();
}

void Global2Local::recordGlobals() {
    map<Function*, int> func2idx; //给每一个函数一个编号，这种关系存储在map中
    int idx = 0;
    //遍历每一个函数
    for (auto it = unit->begin(); it != unit->end(); it++) {
        func2idx[*it] = idx++;
        //遍历每一个基本块
        for (auto block : (*it)->getBlockList())
            //遍历基本块每一条指令
            for (auto in = block->begin(); in != block->end(); in = in->getNext())
                //遍历找use为全局操作数的指令，插入我们的各map中
                for (auto u : in->getUse())
                    if (u->isGlobal()) {
                        auto entry = u->getEntry();
                        globals[entry][*it].push_back(in);
                        usedGlobals[*it].insert(entry);
                        if (in->isLoad())
                            read[*it].insert(entry);
                        else if (in->isStore())
                            write[*it].insert(entry);
                    }
    }
    // printAllRecord();
    // 初始化一个二维的matrix, 行idx,列idx,且值为0
    // idx就是函数的总个数，猜测这个矩阵用来表示函数之间的调用关系
    vector<vector<int>> matrix(idx, vector<int>(idx));
    //遍历每一个函数，我们这边getPreds获取这个函数的前继，也就是调用了当前函数的那些call语句
    //组织形式为map<Function*, std::vector<Instruction*>>，因此fist为“调用了当前函数的那些函数”
    for (auto it : func2idx)
        for (auto it1 : it.first->getCallPred()){
            Function* funcPred=it1->getParent()->getParent();
            //比如编号为3的函数里面有一个call调用了编号为1的函数，那么matrix[3][1]=1
            matrix[func2idx[funcPred]][it.second] = 1;            
        }
    //outDeg记录每一个函数均调用了多少个其他的函数
    vector<int> outDeg(idx, 0);
    for (int i = 0; i < idx; i++) {
        //这边把递归函数清空（自己调用自己）？
        matrix[i][i] = 0;
        //求和，存储第i个函数调用的其他函数的总数
        outDeg[i] = accumulate(matrix[i].begin(), matrix[i].end(), 0);
    }
    //finish表示已经处理的函数的个数
    int finish = 0;
    while (finish < idx) {
        //i从0开始，作为一个索引，当找到满足outDeg[i] == 0跳出
        int i;
        for (i = 0; i < idx; i++)
            if (outDeg[i] == 0)
                break;
        //这个函数如果没有调用其他任何，就记outDeg为-1。
        outDeg[i] = -1;
        //已经处理的函数数+1
        finish++;
        //找到这个函数
        auto func = *(unit->begin() + i);
        //找到调用了当前函数的所有其他的函数，让他们的read和write中都存入被调函数的所有read&write
        //可以理解为，当前函数3调用了函数1，那么执行函数3时，会经历函数1中的执行（囊括其read&write）
        for (auto it : func->getCallPred()) {
            Function* funcPred=it->getParent()->getParent();
            read[funcPred].insert(read[func].begin(), read[func].end());
            write[funcPred].insert(write[func].begin(), write[func].end());
            outDeg[func2idx[funcPred]]--;//这种调用关系处理掉了1
        }
    }
    
    //main函数的store清空，why？？？？？？？？？？？？？？？
    // printAllRecord();
    write[unit->getMain()].clear();
    printAllRecord();
}

void Global2Local::printAllRecord() {
    //打印globals
    cout<<"globals:"<<endl;
    for(auto it:globals){
        cout<<it.first->toStr()<<" :: "<<endl;
        for(auto it1:it.second){
            cout<<it1.first->getSymPtr()->toStr()<<" : ";
            for(auto it2:it1.second){
                if(it2->getDef()) cout<<it2->getDef()->toStr()<<" ";
                else cout<<"store ";
            }
            cout<<endl;            
        }
    }
    cout<<endl;
    //打印usedGlobals
    cout<<"usedGlobals:"<<endl;
    for(auto it:usedGlobals){
        cout<<it.first->getSymPtr()->toStr()<<" : ";
        for(auto it1:it.second){
            cout<<it1->toStr()<<" ";
        }
        cout<<endl;
    }
    cout<<endl;
    //打印read
    cout<<"read:"<<endl;
    for(auto it:read){
        cout<<it.first->getSymPtr()->toStr()<<" : ";
        for(auto it1:it.second){
            cout<<it1->toStr()<<" ";
        }
        cout<<endl;
    }
    cout<<endl;
    //打印write
    cout<<"write:"<<endl;
    for(auto it:write){
        if(it.first){
            cout<<it.first->getSymPtr()->toStr()<<" : ";
            if(!it.second.empty()){
                for(auto it1:it.second){
                    cout<<it1->toStr()<<" ";
                }
            }            
        }
        cout<<endl;
    }
    cout<<endl;
}
