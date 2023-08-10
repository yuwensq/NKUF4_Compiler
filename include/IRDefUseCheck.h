#ifndef _IR_DEF_USE_CHECK_H__
#define _IR_DEF_USE_CHECK_H__

#include "Unit.h"
#include <string>
#include <unordered_set>
#include <unordered_map>

class DefUseCheck
{
private:
    Unit *unit;
    std::unordered_set<Operand *> all_def;
    std::unordered_map<Operand *, int> all_use;

public:
    DefUseCheck(Unit *unit) : unit(unit) {}
    ~DefUseCheck() {}
    void pass(std::string passName);
};

#endif