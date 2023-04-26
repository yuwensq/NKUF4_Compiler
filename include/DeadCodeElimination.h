#ifndef __DEAD_CODE_ELIMINATION_H__
#define __DEAD_CODE_ELIMINATION_H__

#include "Unit.h"

class DeadCodeElimination {

private:
    Unit* unit;
    std::vector<Instruction*> worklist; //关键指令
    //与store是否保留有关的一些重要操作数
    std::set<Operand*> gepOp; //load,src是数组的首地址op
    std::set<Operand*> gloOp; //load,src是全局的op
    std::set<Operand*> allocOp; //load,src的def是一个alloc语句
public:
    DeadCodeElimination(Unit* unit) : unit(unit){};
    void pass();
    void initalize(Function* function);
    void mark(Function* function);
    bool remove(Function* function);
    void adjustBlock(Function* function);

    void addCriticalOp(Instruction* ins);
    void markBasic(Function* function);
    void markStore(Function* function);
};

#endif