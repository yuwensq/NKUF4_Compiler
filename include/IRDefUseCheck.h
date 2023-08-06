#pragma once
#include "Unit.h"
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
    void pass()
    {
        for (auto func_it = unit->begin(); func_it != unit->end(); func_it++)
        {
            auto &func = *func_it;
            for (auto bb_it = func->begin(); bb_it != func->end(); bb_it++)
            {
                auto &bb = *bb_it;
                for (auto inst = bb->begin(); inst != bb->end(); inst = inst->getNext())
                {
                    auto def = inst->getDef();
                    if (def == nullptr)

                    {
                        Log("Be lack of Def.");
                    }
                    else
                    {
                        if (all_def.count(def))

                        {
                            Log("Repeated assign.");
                        }
                        else
                        {
                            all_def.insert(def);
                        }
                    }
                    for (auto &&op : inst->getUse())

                    {
                        all_use[op]++;
                    }
                }
            }
            for (auto &&def : all_def)
            {
                if (all_use[def] != def->getUse().size())

                {
                    Log("Uses are not corresponding.");
                }
            }
            all_def.clear();
            all_use.clear();
        }
    }
};
