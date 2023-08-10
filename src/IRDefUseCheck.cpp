#include "IRDefUseCheck.h"

void DefUseCheck::pass(std::string passName)
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
                if (def == nullptr) // 这个有可能的，比如call指令def就可能使nullptr
                {
                    // Log("Be lack of Def.");
                }
                else
                {
                    if (all_def.count(def))
                    {
                        Assert(false, "%s Repeated assign.", passName.c_str());
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
                for (auto use : def->getUse())
                    use->output();
                Log("%s Uses are not corresponding. %%t%d %d %d", passName.c_str(), static_cast<TemporarySymbolEntry *>(def->getEntry())->getLabel(), all_use[def], def->getUse().size());
            }
        }
        all_def.clear();
        all_use.clear();
    }
}