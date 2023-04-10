#ifndef __IRCOMSUBEXPRELIM_H__
#define __IRCOMSUBEXPRELIM_H__

#include "Unit.h"
#include "PureFunctionAnalyser.h"

class IRComSubExprElim
{
private:
    Unit *unit;
    PureFunctionAnalyser *pfa;
    std::vector<std::pair<Instruction *, Operand *>> addedLoad;

    void insertLoadAfterStore();
    void removeLoadAfterStore();
    void doCSE();
    Instruction *getSrc(Operand *, std::string &);
    /**
     * 判断load指令是否被无效了，就是再往前找也没用了
     */
    bool invalidate(Instruction *, Instruction *);
    /**
     * 判断两个指令是否是相同的表达式
     */
    bool isSameExpr(Instruction *, Instruction*);
    /***
     * 判断一条指令是否需要往前找有没有出现相同的表达式
     */
    bool skip(Instruction *);
    /***
     * 用于局部公共子表达式删除，
     * 找到一条指令在一个基本块中的上一条相同的表达式指令，没有返回nullptr
     */
    Instruction *preSameExpr(Instruction *);
    /***
     * 对一个函数进行局部公共子表达式删除，
     * 返回true表示已经收敛，一趟下去没有发现可以删除的
     */
    bool localCSE(Function *);
    /***
     * 对一个函数进行全局公共子表达式删除，
     * 返回true表示已经收敛，一趟下去没有发现可以删除的
     */
    bool globalCSE(Function *);

public:
    IRComSubExprElim(Unit *unit);
    ~IRComSubExprElim();
    void pass();
};

#endif