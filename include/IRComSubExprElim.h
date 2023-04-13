#ifndef __IRCOMSUBEXPRELIM_H__
#define __IRCOMSUBEXPRELIM_H__

#include "Unit.h"
#include "PureFunctionAnalyser.h"
#include <unordered_map>

struct Expr
{
    Instruction *inst;
    std::set<Instruction *> srcs;
    Expr(Instruction *inst) : inst(inst){};
    // 定义这个用来调用find函数
    bool operator==(const Expr &other) const
    {
        // 这里要倒着来，就是两个指令相等，返回false，不等返回true
        auto inst1 = inst;
        auto inst2 = other.inst;
        if (inst1->getType() != inst2->getType() || inst1->getOpCode() != inst2->getOpCode())
            return false;
        auto ops1 = inst1->getUse();
        auto ops2 = inst2->getUse();
        if (ops1.size() != ops2.size())
            return false;
        auto op2 = ops2.begin();
        for (auto op1 : ops1)
        {
            auto se1 = op1->getEntry();
            auto se2 = (*op2)->getEntry();
            if (se1->isConstant() && se2->isConstant())
            {
                if (se1->getType()->isFloat() && se2->getType()->isFloat())
                {
                    if (static_cast<float>(static_cast<ConstantSymbolEntry *>(se1)->getValue()) != static_cast<float>(static_cast<ConstantSymbolEntry *>(se2)->getValue()))
                        return false;
                }
                else if (se1->getType()->isInt() && se2->getType()->isInt())
                {
                    if (static_cast<int>(static_cast<ConstantSymbolEntry *>(se1)->getValue()) != static_cast<int>(static_cast<ConstantSymbolEntry *>(se2)->getValue()))
                        return false;
                }
                else
                    return false;
            }
            else if (op1 != (*op2))
                return false;
            op2++;
        }
        return true;
    };
};

class IRComSubExprElim
{
private:
    Unit *unit;
    PureFunctionAnalyser *pfa;
    std::unordered_map<Instruction *, Instruction *> gep2Alloc; // 这个变量用来加速getSrc函数
    std::vector<std::pair<Instruction *, Instruction *>> addedLoad;

    std::vector<Expr> exprVec;
    // 指令to表达式
    std::map<Instruction *, int> ins2Expr;
    // 这个变量结合genBB使用，记录一个基本块中gen的表达式的结果寄存器
    std::map<BasicBlock *, std::map<int, Operand *>> expr2Op;
    std::map<BasicBlock *, std::set<int>> genBB;
    std::map<BasicBlock *, std::set<int>> killBB;
    std::map<BasicBlock *, std::set<int>> inBB;
    std::map<BasicBlock *, std::set<int>> outBB;
    std::map<BasicBlock *, std::map<int, std::set<std::pair<BasicBlock *, Operand *>>>> inBBOp;
    std::map<BasicBlock *, std::map<int, Operand *>> outBBOp;

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
    bool isSameExpr(Instruction *, Instruction *);
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
    /**
     * 判断一个load指令会不会被他之后的指令kill
     */
    bool isKilled(Instruction *inst);
    /**
     * 数据流分析，计算每个基本块的gen集合和set集合
     */
    void calGenAndKill(Function *);
    /**
     * 求两个集合的交集，stl那个不太好用，封装一下
     */
    std::set<int> intersection(std::set<int> &, std::set<int> &);
    /**
     * 数据流分析，计算每个基本块的in集合和out集合
     */
    void calInAndOut(Function *);
    /**
     * 计算完gen kill in out之后就可以删除全局公共子表达式了
     */
    bool removeGlobalCSE(Function *);
    /***
     * 对一个函数进行全局公共子表达式删除，
     * 返回true表示已经收敛，一趟下去没有发现可以删除的
     */
    bool globalCSE(Function *);
    /**
     * 清除所有数据结构，以防进行好多遍pass
     */
    void clearData();

public:
    IRComSubExprElim(Unit *unit);
    ~IRComSubExprElim();
    void pass();
};

#endif