#include "Global2Local.h"
#include <numeric>
using namespace std;

void Global2Local::pass() {
    recordGlobals();
    auto iter = unit->begin();
    while (iter != unit->end())
        pass(*iter++);
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
    
    //main函数的store清空，why？
    // printAllRecord();
    write[unit->getMain()].clear();
    printAllRecord();
}

//对每一个函数执行pass
void Global2Local::pass(Function* func) {
    //funcGlobals为这个函数直接出现的那个全局变量（store&load&gep）
    auto funcGlobals = usedGlobals[func];
    if (funcGlobals.empty())
        return;
    //g2l是这样的，这个map中，前面是存的那个全局符号表项，后面存的要替换它的地址操作数
    map<SymbolEntry*, Operand*> g2l;
    //step1：遍历globals中每一个符号表项，在enrty插入alloc-load-store，替换原函数中load和store的操作数
    for (auto g : funcGlobals) {
        //cout<<func->getSymPtr()->toStr()<<" : "<<g->toStr()<<endl;
        //如果是常量变量或常数组的话，不处理，也就是全局声明 const a=2；不处理它（交给后面优化）
        if (((IdentifierSymbolEntry*)g)->getConstant()) {
            continue;
        }
        //我们load或store全局的时候，是用指针去取值的
        auto type = ((PointerType*)(g->getType()))->getType();
        //先不处理全局数组，也就是现在我们就关注全局的变量（int&float)
        if (type->isArray())
            continue;
        //获取有这个全局变量的指令集合
        auto ins = this->globals[g][func];
        //接下来我们要将有全局的指令进行转化，拆分成若干条其他的指令（无关全局）
        auto newSe = new TemporarySymbolEntry(new PointerType(type), SymbolTable::getLabel());
        auto newAddr = new Operand(newSe);
        g2l[g] = newAddr;
        auto entry = func->getEntry();
        // 函数入口load 出口store 
        // 倘若调了其他函数 （1）如果用了store过的global为参，调用前需store （2）若被调函数store global，返回到本函数需load
        // 这里需要随便来一个se type对就行
        // 插入指令：alloc-load-store在entry的最开始
        auto alloc = new AllocaInstruction(newAddr, new TemporarySymbolEntry(type, 0));
        alloc->setParent(entry);
        entry->insertFront(alloc,false);
        SymbolEntry* addr_se;
        addr_se = new IdentifierSymbolEntry(*((IdentifierSymbolEntry*)g));
        addr_se->setType(new PointerType(type));
        auto addr = new Operand(addr_se);
        auto dst = new Operand(new TemporarySymbolEntry(type, SymbolTable::getLabel()));
        auto load = new LoadInstruction(dst, addr);
        load->setParent(entry);
        for (auto in = entry->begin(); in != entry->end(); in = in->getNext())
            if (!in->isAlloc()) {
                entry->insertBefore(load, in);
                break;
            }
        auto store = new StoreInstruction(newAddr, dst);
        store->setParent(entry);
        // entry->insertBefore(load, store);
        entry->insertAfter(store, load);

        //替换原函数中的load和store指令
        for (auto in : ins) {
            if (in->isLoad()) {
                auto addr = in->getUse()[0];
                in->replaceUse(addr, newAddr);
            } else if (in->isStore()) {
                auto addr = in->getUse()[0];
                in->replaceUse(addr, newAddr);
            } else
                assert(0);
        }
    }
    //处理非main函数中的ret：找到这个函数的每一条返回语句，在它之前又插入load-store
    for (auto block : func->getBlockList()) {
        auto in = block->rbegin();
        if (in->isRet())
            //遍历这个函数所有store
            //write对应store全局的指令，考虑call调用的其他函数有的write，但把main函数清空
            //也是考虑到main函数的ret其实我们不用考虑，因为不会有函数调用main，是否这样理解？
            for (auto it : write[func]) {
                if (!g2l[it])
                    continue;
                auto type = ((PointerType*)(it->getType()))->getType();
                //仍不考虑数组
                if (type->isArray())
                    continue;
                auto addr_se = new IdentifierSymbolEntry(*((IdentifierSymbolEntry*)it));
                addr_se->setType(new PointerType(type));
                auto addr = new Operand(addr_se);
                auto dst = new Operand( new TemporarySymbolEntry(type, SymbolTable::getLabel()));
                auto load = new LoadInstruction(dst, g2l[it]);
                load->setParent(block);
                block->insertBefore(load, in);
                auto store = new StoreInstruction(addr, dst);
                store->setParent(block);
                block->insertBefore(store, in);
            }
    }
    //处理所有call指令：如果被调函数要读某个全局，那么调用前要load+store；如果要写某个全局，那么调用后要load+store
    for (auto block : func->getBlockList()) {
        for (auto in = block->begin(); in != block->end(); in = in->getNext()) {
            if (in->isCall()) {
                //获取被调函数f
                auto f = ((IdentifierSymbolEntry*)(((CallInstruction*)in)->getFunc()))->getFunction();
                if (!f)
                    continue;
                //遍历f中每一个load相关的全局操作数g：
                for (auto g : read[f]) {
                    auto type = ((PointerType*)(g->getType()))->getType();
                    if (type->isArray())
                        continue;
                    // 此时函数没有用到g 也就没必要多这一步了
                    if (!g2l[g])
                        continue;
                    auto addr_se = new IdentifierSymbolEntry(*((IdentifierSymbolEntry*)g));
                    addr_se->setType(new PointerType(type));
                    auto addr = new Operand(addr_se);
                    auto dst = new Operand(new TemporarySymbolEntry(type, SymbolTable::getLabel()));
                    auto load = new LoadInstruction(dst, g2l[g]);
                    load->setParent(block);
                    block->insertBefore(load, in);
                    auto store = new StoreInstruction(addr, dst);
                    store->setParent(block);
                    block->insertBefore(store, in);
                }
                //遍历g中每一个store相关的全局操作数g：
                for (auto g : write[f]) {
                    auto type = ((PointerType*)(g->getType()))->getType();
                    if (type->isArray())
                        continue;
                    if (!g2l[g])
                        continue;
                    auto addr_se = new IdentifierSymbolEntry(*((IdentifierSymbolEntry*)g));
                    addr_se->setType(new PointerType(type));
                    auto addr = new Operand(addr_se);
                    auto dst = new Operand(new TemporarySymbolEntry(type, SymbolTable::getLabel()));
                    auto load = new LoadInstruction(dst, addr);
                    load->setParent(block);
                    auto store = new StoreInstruction(g2l[g], dst);
                    store->setParent(block);
                    // block->insertBefore(in, store);
                    // block->insertBefore(in, load);                    
                    block->insertAfter(store, in);
                    block->insertAfter(load, in);
                }
            }
        }
    }
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
