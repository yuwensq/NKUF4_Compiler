#ifndef __PUREFUNCTIONANALYSER_H__
#define __PUREFUNCTIONANALYSER_H__

#include "Unit.h"
#include "SymbolTable.h"
#include <map>
#include <set>
#include <vector>
#include <string>

class PureFunctionAnalyser
{
private:
    Unit *unit;
    // caller记录每个函数的调用者
    std::map<Function *, std::set<Function *>> caller;
    // callee记录每个函数调用的函数，这里先不记录对于sysy的调用，这里要注意
    std::map<Function *, std::set<Function *>> callee;
    // 判断函数纯不纯
    std::map<Function *, bool> funcIsPure;
    // 保存一个函数会修改的全局变量信息，这里保存全局变量的名字，需要的话从global里找表项得了
    // 因为代码里面只有全局变量的指针，找不到实际表示全局变量的那个表项（框架的锅）
    std::map<Function *, std::set<std::string>> funcStoreGlobalVars;
    /***
     * 获取函数之间的调用关系，即构建caller和callee这两个map
     */
    void analyseCallRelation();
    /***
     * 只根据函数内部直接的store和sysy函数调用判断这个函数纯不纯，并返回判断结果
     */
    bool analyseFuncByStoreAndSysy(Function *);
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
};

#endif