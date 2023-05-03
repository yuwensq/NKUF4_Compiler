#ifndef __GLOBAL2LOCAL_H__
#define __GLOBAL2LOCAL_H__

#include "Unit.h"

using namespace std;

class Global2Local {
    Unit* unit;
    map<SymbolEntry*, map<Function*, vector<Instruction*>>> globals; //构建全局符号表项-所在函数-对应指令的关系
    map<Function*, set<SymbolEntry*>> usedGlobals; //load+store，所有使用的全局操作数，不考虑call
    map<Function*, set<SymbolEntry*>> read; //对应load全局的指令,考虑call调用的其他函数有的read
    map<Function*, set<SymbolEntry*>> write;//对应store全局的指令，考虑call调用的其他函数有的write，但把main函数清空

   public:
    Global2Local(Unit* unit) : unit(unit){};
    void pass();
    void recordGlobals();
    void pass(Function* function);
    void unstoreGlobal2Const();
};

#endif