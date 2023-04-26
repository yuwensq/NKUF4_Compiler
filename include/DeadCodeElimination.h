#ifndef __DEAD_CODE_ELIMINATION_H__
#define __DEAD_CODE_ELIMINATION_H__

#include "Unit.h"

class DeadCodeElimination {

private:
    Unit* unit;
    std::vector<Instruction*> worklist; //关键指令
    std::set<std::vector<Operand*>> loadArrOp; //load,src是数组的若干个op
    std::set<Operand*> loadGloOp; //load,src是全局的op
    std::set<Operand*> loadAllocOp; //load,src的def是一个alloc语句
public:
    DeadCodeElimination(Unit* unit) : unit(unit){};
    void pass();
    void initalize(Function* function);
    void mark(Function* function);
    bool remove(Function* function);
    void adjustBlock(Function* function);

    void addLoadOp(Instruction* ins);
    void markBasic(Function* function);
    void markStore(Function* function);
    bool isequal(std::vector<Operand*>& op1,std::vector<Operand*>& op2);

};

#endif