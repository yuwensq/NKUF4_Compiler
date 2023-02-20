#ifndef __UNIT_H__
#define __UNIT_H__

#include <vector>
#include "Function.h"
#include "AsmBuilder.h"

class Unit
{
    typedef std::vector<Function *>::iterator iterator;
    typedef std::vector<Function *>::reverse_iterator reverse_iterator;

private:
    std::vector<Function *> func_list;
    // 全局变量
    std::vector<SymbolEntry *> global_vars;
    // 运行时函数
    std::vector<SymbolEntry *> declare_list;

public:
    Unit() = default;
    ~Unit();
    void insertFunc(Function *);
    void removeFunc(Function *);
    void output() const;
    iterator begin() { return func_list.begin(); };
    iterator end() { return func_list.end(); };
    reverse_iterator rbegin() { return func_list.rbegin(); };
    reverse_iterator rend() { return func_list.rend(); };
    void addGlobalVar(SymbolEntry *se) { global_vars.push_back(se); };
    void insertDeclare(SymbolEntry *se)
    {
        if (std::find(declare_list.begin(), declare_list.end(), se) == declare_list.end())
        {
            declare_list.push_back(se);
        }
    };
    void genMachineCode(MachineUnit *munit);
};

#endif