#ifndef __MEM2REG_H__
#define __MEM2REG_H__

#include "Unit.h"
#include <assert.h>
#include <stack>
#include <queue>
#include <map>
#include <set>

/**
 * 1. 对于每个基本块B，找到其所有前驱基本块Preds。
2. 对于每个变量x，遍历Preds中所有基本块，找到x的最新定义，并将其记录在字典LatestDefs[x]中。
3. 对于每个基本块B，遍历其中所有的Phi节点。
4. 对于每个Phi节点[x, B1, B2...]中的变量x，从LatestDefs[x]中获取其最新定义，并将其添加到Phi节点的[x, B, latest_def]中。
*/
/**
 * 1. 遍历所有基本块，找到所有的加载指令和Phi节点，以及它们所使用的内存变量。
2. 对于每个使用内存变量的基本块，创建一个新的寄存器变量，并将其添加到函数中。
3. 遍历每个基本块，对于所有使用内存变量的指令，将其替换为对应的寄存器变量。
4. 遍历Phi节点，对于所有使用内存变量的条目，将其替换为对应的寄存器变量。
5. 删除不再使用的内存变量。
*/
class Mem2Reg
{
private:
    Unit *unit;
    // std::map<BasicBlock *, std::set<BasicBlock *>> IDom;
    // void ComputeDom(Function *);
    // std::map<BasicBlock *, std::set<BasicBlock *>> DF;
    // void ComputeDomFrontier(Function *);
    // void InsertPhi(Function *);
    // void Rename(Function *);
    std::vector<AllocaInstruction*> allocaIns;
    std::map<Operand*, std::stack<Operand*>> stacks;
    std::vector<BinaryInstruction*> addZeroIns;
    void insertPhiInstruction(Function* function);
    void rename(BasicBlock* block);
    void rename(Function* function);
    Operand* newName(Operand* old);
    void cleanAddZeroIns(Function* function);
    void checkCondBranch(Function* function);

public:
    Mem2Reg(Unit *unit) : unit(unit) {}
    void pass();
};

#endif