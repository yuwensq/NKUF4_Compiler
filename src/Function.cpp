#include "Function.h"
#include "Unit.h"
#include "Type.h"
#include <list>

using namespace std;
extern FILE *yyout;

Function::Function(Unit *u, SymbolEntry *s)
{
    u->insertFunc(this);
    entry = new BasicBlock(this);
    sym_ptr = s;
    parent = u;
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
