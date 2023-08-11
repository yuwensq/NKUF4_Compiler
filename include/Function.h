#ifndef __FUNCTION_H__
#define __FUNCTION_H__

#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <iostream>
#include "BasicBlock.h"
#include "SymbolTable.h"
#include "AsmBuilder.h"

class Unit;

struct TreeNode
{
    // num only use for dfs tree node
    static int Num;
    int num;
    BasicBlock *block;
    std::vector<TreeNode *> children;
    TreeNode *parent = nullptr;
    // only use for dfs tree node
    TreeNode(BasicBlock *block) : block(block)
    {
        num = Num++;
        block->order = num;
    }
    // only use for dom tree node
    TreeNode(BasicBlock *block, int num) : block(block)
    {
        this->num = block->order;
    }
    void addChild(TreeNode *child) { children.push_back(child); }
    // only use for dom tree node
    int getHeight()
    {
        int height = 0;
        TreeNode *temp = this;
        while (temp)
        {
            height++;
            temp = temp->parent;
        }
        return height;
    }
    bool isAncestor(TreeNode *node)
    {
        auto cur = this;
        while (cur)
        {
            if (cur == node)
                return true;
            cur = cur->parent;
        }
        return false;
    }
};

class Function
{
    typedef std::vector<BasicBlock *>::iterator iterator;
    typedef std::vector<BasicBlock *>::reverse_iterator reverse_iterator;

private:
    std::vector<BasicBlock *> block_list;
    SymbolEntry *sym_ptr;
    BasicBlock *entry;
    Unit *parent;
    std::vector<Operand *> params;        // 存函数的参数
    std::vector<Instruction *> callPreds; // 调用了当前函数的那些call指令
    bool recur;                           // 表征是否有递归调用
    int critical = -1;                    // 表征函数是否关键,-1表示没有被标识

public:
    Function(Unit *, SymbolEntry *);
    ~Function();
    void insertBlock(BasicBlock *bb) { block_list.push_back(bb); };
    BasicBlock *getEntry() { return entry; };
    void setEntry(BasicBlock *bb) { entry = bb; }
    void remove(BasicBlock *bb);
    void strongRemove(BasicBlock *bb);
    void output() const;
    std::vector<BasicBlock *> &getBlockList() { return block_list; };
    iterator begin() { return block_list.begin(); };
    iterator end() { return block_list.end(); };
    reverse_iterator rbegin() { return block_list.rbegin(); };
    reverse_iterator rend() { return block_list.rend(); };
    SymbolEntry *getSymPtr() { return sym_ptr; };
    void addParam(Operand *param) { params.push_back(param); }
    std::vector<Operand *> &getParams() { return params; }
    void genMachineCode(AsmBuilder *);
    int getParamNumber(Operand *param);
    void addCallPred(Instruction *in);
    void removeCallPred(Instruction *in);
    std::vector<Instruction *> &getCallPred() { return callPreds; };
    int getCritical();
    BasicBlock *getMarkBranch(BasicBlock *block);
    Unit *getParent() { return parent; };

public:
    TreeNode *DFSTreeRoot;
    TreeNode *domTreeRoot;
    // preOrder2DFS order-> dfs tree node
    std::vector<TreeNode *> preOrder2DFS;
    // preOrder2dom order-> dom tree node
    std::vector<TreeNode *> preOrder2dom;
    // sdoms idoms order->order
    std::vector<int> sdoms;
    std::vector<int> idoms;

    void computeDFSTree();
    void search(TreeNode *node, bool *visited);
    int getIndex(BasicBlock *block)
    {
        return std::find(block_list.begin(), block_list.end(), block) -
               block_list.begin();
    }
    int eval(int i, int *ancestors);
    void computeSdom();
    int LCA(int i, int j);
    void computeIdom();
    void computeDomFrontier();
    TreeNode *getDomNode(BasicBlock *b) { return preOrder2dom[b->order]; }
    void computeRDF();
    void computeRDFSTree(BasicBlock *exit);
    void computeRSdom(BasicBlock *exit);
    void computeRIdom(BasicBlock *exit);
    void reverseSearch(TreeNode *node, bool *visited);

    std::vector<BasicBlock *> getReversedTopsort();
    void rTopDFS(std::vector<BasicBlock *> &, BasicBlock *, std::map<BasicBlock *, bool> &);
    void genByDFS(std::vector<BasicBlock *> &, std::unordered_set<BasicBlock *> &, BasicBlock *, std::unordered_set<BasicBlock *> &);
    void genReversedTopsort_N_LoopHeader(std::vector<BasicBlock *> &, std::unordered_set<BasicBlock *> &);

    void de_phi();
};

#endif
