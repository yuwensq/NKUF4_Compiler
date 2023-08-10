#ifndef __BASIC_BLOCK_H__
#define __BASIC_BLOCK_H__
#include <vector>
#include <set>
#include "Instruction.h"
#include "AsmBuilder.h"

class Function;

class BasicBlock
{
    typedef std::set<BasicBlock *>::iterator bb_iterator;

private:
    // 这里设置为set可以去重
    std::set<BasicBlock *> pred, succ;
    Instruction *head;
    Function *parent;
    int no;
    bool mark;

public:
    BasicBlock(Function *);
    ~BasicBlock();
    void insertFront(Instruction *, bool);
    void insertBack(Instruction *);
    void insertBefore(Instruction *dst, Instruction *src);
    void insertAfter(Instruction *dst, Instruction *src);
    void remove(Instruction *);
    void strongRemove(Instruction *);
    bool empty() const { return head->getNext() == head; }
    void output() const;
    bool succEmpty() const { return succ.empty(); };
    bool predEmpty() const { return pred.empty(); };
    void addSucc(BasicBlock *);
    void removeSucc(BasicBlock *);
    void cleanAllSucc();
    void addPred(BasicBlock *);
    void removePred(BasicBlock *);
    int getNo() { return no; };
    std::vector<BasicBlock *> getPred() { return std::vector<BasicBlock *>(pred.begin(), pred.end()); };
    std::vector<BasicBlock *> getSucc() { return std::vector<BasicBlock *>(succ.begin(), succ.end()); };
    std::set<BasicBlock *> &getPredRef() { return pred; }
    std::set<BasicBlock *> &getSuccRef() { return succ; }
    Function *getParent() { return parent; };
    Instruction *begin() { return head->getNext(); };
    Instruction *end() { return head; };
    Instruction *rbegin() { return head->getPrev(); };
    Instruction *rend() { return head; };
    bb_iterator succ_begin() { return succ.begin(); };
    bb_iterator succ_end() { return succ.end(); };
    bb_iterator pred_begin() { return pred.begin(); };
    bb_iterator pred_end() { return pred.end(); };
    int getNumOfPred() const { return pred.size(); };
    int getNumOfSucc() const { return succ.size(); };
    void genMachineCode(AsmBuilder *);
    void unsetMark() { mark = false; };
    void setMark() { mark = true; };
    bool getMark() { return mark; };
    void cleanAllMark();

public:
    int order;
    std::set<BasicBlock *> domFrontier;
};

#endif
