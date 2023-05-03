#include "Function.h"
#include "Unit.h"
#include "Type.h"
#include <list>
#include "PureFunctionAnalyser.h"

using namespace std;
extern FILE *yyout;

Function::Function(Unit *u, SymbolEntry *s)
{
    entry = new BasicBlock(this);
    ((IdentifierSymbolEntry*)s)->setFunction(this);
    sym_ptr = s;
    parent = u;
    u->insertFunc(this);
}

Function::~Function()
{
    //     auto delete_list = block_list;
    //     for (auto &i : delete_list)
    //         delete i;
    //     parent->removeFunc(this);
}

// remove the basicblock bb from its block_list.
void Function::remove(BasicBlock *bb)
{
    block_list.erase(std::find(block_list.begin(), block_list.end(), bb));
}

void Function::output() const
{
    FunctionType *funcType = dynamic_cast<FunctionType *>(sym_ptr->getType());
    Type *retType = funcType->getRetType();
    fprintf(yyout, "define %s %s(", retType->toStr().c_str(), sym_ptr->toStr().c_str());
    if (params.size() > 0)
    {
        for (long unsigned int i = 0; i < params.size(); i++)
        {
            if (i != 0)
            {
                fprintf(yyout, ", ");
            }
            fprintf(yyout, "%s %s", params[i]->getType()->toStr().c_str(), params[i]->toStr().c_str());
        }
    }
    fprintf(yyout, ") {\n");
    std::set<BasicBlock *> v;
    std::list<BasicBlock *> q;
    Log("%d", block_list.size());
    q.push_back(entry);
    v.insert(entry);
    while (!q.empty())
    {
        auto bb = q.front();
        q.pop_front();
        bb->output();
        for (auto succ = bb->succ_begin(); succ != bb->succ_end(); succ++)
        {
            if (v.find(*succ) == v.end())
            {
                v.insert(*succ);
                q.push_back(*succ);
            }
        }
    }
    fprintf(yyout, "}\n");
}

void Function::genMachineCode(AsmBuilder *builder)
{
    auto cur_unit = builder->getUnit();
    auto cur_func = new MachineFunction(cur_unit, this->sym_ptr);
    builder->setFunction(cur_func);
    std::map<BasicBlock *, MachineBlock *> map;
    for (auto block : block_list)
    {
        block->genMachineCode(builder);
        map[block] = builder->getBlock();
    }
    // Add pred and succ for every block
    for (auto block : block_list)
    {
        auto mblock = map[block];
        for (auto pred = block->pred_begin(); pred != block->pred_end(); pred++)
            mblock->addPred(map[*pred]);
        for (auto succ = block->succ_begin(); succ != block->succ_end(); succ++)
            mblock->addSucc(map[*succ]);
    }
    cur_unit->InsertFunc(cur_func);
}

int Function::getParamNumber(Operand *param)
{
    int i = 0;
    for (auto pa : params)
    {
        if (pa == param)
            return i;
        i++;
    }
    return 0;
}

void Function::addCallPred(Instruction* in) 
{
    assert(in->isCall());
    auto func = in->getParent()->getParent();
    //递归调用
    if (func == this)
        recur = true;
    this->callPreds.push_back(in);
}

void Function::removeCallPred(Instruction* in)
{
    assert(in->isCall());
    auto it = find(callPreds.begin(), callPreds.end(), in);
    assert(it != callPreds.end());
    callPreds.erase(it);
}

PureFunctionAnalyser* pureFunc = nullptr ;        // 检测纯函数
int Function::getCritical()
{
    if(critical!=-1) return critical;
    if(pureFunc==nullptr){
        pureFunc = new PureFunctionAnalyser(parent);
    }
    //auto pureFunc = new PureFunctionAnalyser(parent);
    bool ispure = pureFunc->isPure(this);
    if(!ispure)
    {
        critical = 1;
        return critical;
    }
    else
    {   
        for (auto block : block_list) {
            for (auto it = block->begin(); it != block->end(); it = it->getNext()) {
                //这个函数调用了critical的函数
                if (it->isCall()) {
                    IdentifierSymbolEntry* funcSE =(IdentifierSymbolEntry*)(((CallInstruction*)it)->getFunc());
                    Function* func = funcSE->getFunction();
                    if(this == func) continue; //递归call略去
                    if (funcSE->isSysy() || funcSE->getName() == "llvm.memset.p0i8.i32") {
                        critical = 1;
                        return critical;
                    } else {
                        int subCritical = func->getCritical();
                        if(subCritical==1){
                            critical = 1;
                            return critical;
                        }
                    }
                }
            }
        }
        critical = 0;
        return critical;
    }
}

BasicBlock* Function::getMarkBranch(BasicBlock* block)
{
    set<BasicBlock*> blocks;
    while (true) {
        auto order = idoms[block->order];
        block = preOrder2dom[order]->block;
        if (blocks.count(block))
            return nullptr;
        blocks.insert(block);
        if (block->getMark())
            return block;
    }
}

int TreeNode::Num = 0;
void Function::computeDFSTree()
{
    TreeNode::Num = 0;
    int len = block_list.size();
    preOrder2DFS.resize(len);
    bool *visited = new bool[len]{};
    DFSTreeRoot = new TreeNode(entry);
    preOrder2DFS[DFSTreeRoot->num] = DFSTreeRoot;
    search(DFSTreeRoot, visited);
    delete[] visited;
}

void Function::search(TreeNode *node, bool *visited)
{
    int n = getIndex(node->block);
    visited[n] = true;
    auto block = block_list[n];
    for (auto it = block->succ_begin(); it != block->succ_end(); it++)
    {
        int idx = getIndex(*it);
        if (!visited[idx])
        {
            TreeNode *child = new TreeNode(*it);
            preOrder2DFS[child->num] = child;
            child->parent = node;
            node->addChild(child);
            search(child, visited);
        }
    }
}

int Function::eval(int v, int *ancestors)
{
    int a = ancestors[v];
    while (a != -1 && ancestors[a] != -1)
    {
        if (sdoms[v] > sdoms[a])
            v = a;
        a = ancestors[a];
    }
    return v;
}

void Function::computeSdom()
{
    int len = block_list.size();
    sdoms.resize(len);
    // TODO：
    int *ancestors = new int[len];
    for (int i = 0; i < len; i++)
    {
        sdoms[i] = i;
        ancestors[i] = -1;
    }
    for (auto it = preOrder2DFS.rbegin(); (*it)->block != entry; it++)
    {
        auto block = (*it)->block;
        int s = block->order;
        for (auto it1 = block->pred_begin(); it1 != block->pred_end(); it1++)
        {
            int z = eval((*it1)->order, ancestors);
            if (sdoms[z] < sdoms[s])
                sdoms[s] = sdoms[z];
        }
        ancestors[s] = (*it)->parent->num;
    }
    delete[] ancestors;
}

int Function::LCA(int i, int j)
{
    TreeNode *n1 = preOrder2dom[i];
    TreeNode *n2 = preOrder2dom[j];
    int h1 = n1->getHeight();
    int h2 = n2->getHeight();
    if (h1 > h2)
    {
        swap(h1, h2);
        swap(n1, n2);
    }
    int h = h2 - h1;
    for (int i = 0; i < h; i++)
        n2 = n2->parent;
    while (n1 && n2)
    {
        if (n1 == n2)
            return n1->num;
        n1 = n1->parent;
        n2 = n2->parent;
    }
    return -1;
}

void Function::computeIdom()
{
    int len = block_list.size();
    idoms.resize(len);
    domTreeRoot = new TreeNode(entry, 0);
    preOrder2dom.resize(len);
    preOrder2dom[entry->order] = domTreeRoot;
    idoms[entry->order] = 0;
    for (auto it = preOrder2DFS.begin() + 1; it != preOrder2DFS.end(); it++)
    {
        int p = LCA((*it)->parent->num, sdoms[(*it)->num]);
        idoms[(*it)->num] = p;
        auto parent = preOrder2dom[p];
        TreeNode *node = new TreeNode((*it)->block, 0);
        node->parent = parent;
        parent->addChild(node);
        preOrder2dom[(*it)->num] = node;
    }
}

void Function::computeDomFrontier()
{
    for (auto block : block_list)
    {
        if (block->getNumOfPred() >= 2)
        {
            for (auto it = block->pred_begin(); it != block->pred_end(); it++)
            {
                int runner = (*it)->order;
                while (runner != idoms[block->order])
                {
                    preOrder2DFS[runner]->block->domFrontier.insert(block);
                    runner = idoms[runner];
                }
            }
        }
    }
}

void Function::computeRDFSTree(BasicBlock* exit) {
    TreeNode::Num = 0;
    int len = block_list.size();
    //先根遍历DFS树（Treenode*类型的vector）重置大小
    preOrder2DFS.resize(len);
    bool* visited = new bool[len]{};
    //因为是逆的DFS树，所以根为exit，DFSTreeRoot->num=Num++
    DFSTreeRoot = new TreeNode(exit);
    //这边这个num=0
    preOrder2DFS[DFSTreeRoot->num] = DFSTreeRoot;
    //逆向DFS搜索
    reverseSearch(DFSTreeRoot, visited);
    delete[] visited;
}

//逆向DFS搜索，搭建DFS树preOrder2DFS
void Function::reverseSearch(TreeNode* node, bool* visited) {
    //找到当前的node在block_list中相对begin的偏移n
    int n = getIndex(node->block);
    //标记访问n
    visited[n] = true;
    //这个block不能直接等同于node->block吗
    auto block = block_list[n];
    //遍历所有pre，因为是逆向~
    for (auto it = block->pred_begin(); it != block->pred_end(); it++) {
        int idx = getIndex(*it);
        if (!visited[idx]) {
            TreeNode* child = new TreeNode(*it);
            preOrder2DFS[child->num] = child;
            child->parent = node;
            node->addChild(child);
            reverseSearch(child, visited);
        }
    }
}

//semidominator，简称为sdom(w)，可以看做是对idom的一种逼近。
void Function::computeRSdom(BasicBlock* exit) {
    int len = block_list.size();
    //sdoms和idoms都是int列表
    sdoms.resize(len);
    int* ancestors = new int[len];
    //sdoms初始从0到len-1；ancestors初始为-1
    for (int i = 0; i < len; i++) {
        sdoms[i] = i;
        ancestors[i] = -1;
    }
    //根据算法，需要以reverse preorder的方式处理每一个节点
    for (auto it = preOrder2DFS.rbegin(); (*it)->block != exit; it++) {
        auto block = (*it)->block;
        int s = block->order;
        //遍历当前基本块的每一个后继（也就是逆向DFS的每一个前驱）
        //情况1：小于s；情况2：大于s，设为q，递归地求sdom(sdom(...(sdom(q)))直到<s
        //取上面两种的最小值
        for (auto it1 = block->succ_begin(); it1 != block->succ_end(); it1++) {
            int z = eval((*it1)->order, ancestors);
            if (sdoms[z] < sdoms[s])
                sdoms[s] = sdoms[z];
        }
        ancestors[s] = (*it)->parent->num;
    }
    delete[] ancestors;
}

//逆向直接支配
void Function::computeRIdom(BasicBlock* exit) {
    int len = block_list.size();
    idoms.resize(len);
    domTreeRoot = new TreeNode(exit, 0);
    preOrder2dom.resize(len);
    preOrder2dom[exit->order] = domTreeRoot;
    idoms[exit->order] = 0;
    for (auto it = preOrder2DFS.begin() + 1; it != preOrder2DFS.end(); it++) {
        int p = LCA((*it)->parent->num, sdoms[(*it)->num]);
        idoms[(*it)->num] = p;
        auto parent = preOrder2dom[p];
        TreeNode* node = new TreeNode((*it)->block, 0);
        node->parent = parent;
        parent->addChild(node);
        preOrder2dom[(*it)->num] = node;
    }
}

//逆向支配边界
void Function::computeRDF() {
    //给这个函数的每一个有ret语句的基本块的后面都链接到同一个exit基本块
    BasicBlock* exit = new BasicBlock(this);
    for (auto b : block_list) {
        if (b->rbegin()->isRet()) {
            b->addSucc(exit);
            exit->addPred(b);
        }
    }
    //逆向DFS（以exit为根DFSTreeRoot），其序列存储到preOrder2DFS，静态变量Num从0开始
    computeRDFSTree(exit);
    computeRSdom(exit);
    computeRIdom(exit);
    for (auto block : block_list) {
        if (block->getNumOfSucc() >= 2) {
            for (auto it = block->succ_begin(); it != block->succ_end(); it++) {
                int runner = (*it)->order;
                while (runner != idoms[block->order]) {
                    preOrder2DFS[runner]->block->domFrontier.insert(block);
                    runner = idoms[runner];
                }
            }
        }
    }
    delete exit;
}

void Function::de_phi()
{
    std::map<BasicBlock *, std::vector<Instruction *>> pcopy;
    auto blocks = std::vector<BasicBlock *>(this->begin(), this->end());
    // Critical Edge Splitting Algorithm for making non-conventional SSA form conventional
    for (auto bb : blocks)
    {
        if (!bb->begin()->isPhi())
            continue;
        auto preds = std::vector<BasicBlock *>(bb->pred_begin(), bb->pred_end());
        for (auto &pred : preds)
        {
            if (pred->getNumOfSucc() > 1)
            {
                BasicBlock *splitBlock = new BasicBlock(this);

                CondBrInstruction *branch = (CondBrInstruction *)(pred->rbegin());
                if (branch->getTrueBranch() == bb)
                    branch->setTrueBranch(splitBlock);
                else
                    branch->setFalseBranch(splitBlock);
                pred->addSucc(splitBlock);
                pred->removeSucc(bb);
                splitBlock->addPred(pred);
                
                new UncondBrInstruction(bb, splitBlock);
                splitBlock->addSucc(bb);
                bb->addPred(splitBlock);
                bb->removePred(pred);
                for (auto i = bb->begin(); i != bb->end() && i->isPhi(); i = i->getNext())
                {
                    auto def = i->getDef();
                    auto src = ((PhiInstruction *)i)->getEdge(pred);
                    src->removeUse(i);
                    pcopy[splitBlock].push_back(new BinaryInstruction(
                        BinaryInstruction::ADD, def, src, new Operand(new ConstantSymbolEntry(def->getType(), 0))));
                }
            }
            else
            {
                for (auto i = bb->begin(); i != bb->end() && i->isPhi(); i = i->getNext())
                {
                    auto def = i->getDef();
                    auto src = ((PhiInstruction *)i)->getEdge(pred);
                    src->removeUse(i);
                    pcopy[pred].push_back(new BinaryInstruction(
                        BinaryInstruction::ADD, def, src, new Operand(new ConstantSymbolEntry(def->getType(), 0))));
                }
            }
        }
        while (bb->begin() != bb->end())
        {
            auto i = bb->begin();
            if (!i->isPhi()) break;
            bb->remove(i);
        }
    }
    
    for (auto &&[block, ins] : pcopy)
    {
        set<Operand*> defs;
        for (auto &in : ins)
            defs.insert(in->getDef());
        vector<Instruction*> temp;
        for (auto it = ins.begin(); it != ins.end();)
        {
            if (defs.count((*it)->getUse()[0]) == 0)
            {
                temp.push_back(*it);
                it = ins.erase(it);
            }
            else
            {
                it++;
            }
        }
        for (auto &in : ins)
        {
            bool flag = false;
            auto def = in->getDef();
            for (auto it = temp.begin(); it != temp.end(); it++)
            {
                if ((*it)->getUse()[0] == def)
                {
                    temp.insert(it + 1, in);
                    flag = true;
                    break;
                }
            }
            if (flag)
                continue;
            temp.insert(temp.begin(), in);
        }
        auto endIns = block->rbegin();
        for (auto &in : temp)
            block->insertBefore(in, endIns);
    }
}

