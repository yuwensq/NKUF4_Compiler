#ifndef __PUREFUNCTIONANALYSER_H__
#define __PUREFUNCTIONANALYSER_H__

#include "Unit.h"
#include "SymbolTable.h"
#include <map>
#include <set>
#include <vector>
#include <string>

/***
 * 这个类主要用于辅助进行公共子表达式删除，纯函数的定义
 * 和严格的纯函数定义有些不一致，这里的纯函数仅仅指的是
 * 函数内部不会对全局变量全局数组以及传入的参数数组进行
 * 修改的函数。
 */
class PureFunctionAnalyser
{
private:
    Unit *unit;
    // caller记录每个函数的调用者
    std::map<Function *, std::set<std::pair<Function *, Instruction *>>> caller;
    // callee记录每个函数调用的函数，这里先不记录对于sysy的调用，这里要注意
    std::map<Function *, std::set<Function *>> callee;
    // 判断函数纯不纯
    std::map<Function *, bool> funcIsPure;
    // 判断函数会不会对传入的参数数组进行修改
    std::map<Function *, bool> changeArgArray;
    std::map<Function *, std::set<int>> changeArgNumber;
    // 保存一个函数会修改的全局变量信息，这里保存全局变量的名字，需要的话从global里找表项得了
    // 因为代码里面只有全局变量的指针，找不到实际表示全局变量的那个表项（框架的锅）
    std::map<Function *, std::set<std::string>> funcChangeGlobalVars;
    /***
     * 获取函数之间的调用关系，即构建caller和callee这两个map
     */
    void analyseCallRelation();
    /***
     * 辅助函数，判断一个指针操作数的源头是不是函数的局部变量，即局部数组
     */
    bool srcIsLocal(Operand *, std::string &);
    int getArgNumber(Operand *);
    /***
     * 先不判断用户定义的函数对该函数纯不纯的影响，而是只看该函数是否写了全局变量，
     * 以及是否调用sysy函数来初步判断这个函数纯不纯，并得到这个函数都写了哪些全局变量
     */
    bool analyseFuncWithoutCallee(Function *);
    /***
     * 对全部函数纯不纯进行分析
     */
    void analyseFunc();

public:
    PureFunctionAnalyser(Unit *);
    /***
     * 判断一个函数是否是纯函数
     */
    bool isPure(Function *func);
    bool changeAArray(Function *func);
    std::set<std::string> &getStoreGlobalVar(Function *func);
    std::set<int> &getChangeArgNum(Function *func);
};

#endif