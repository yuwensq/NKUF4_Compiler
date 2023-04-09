#include "IRComSubExprElim.h"

IRComSubExprElim::IRComSubExprElim(Unit *unit)
{
    this->unit = unit;
    pfa = new PureFunctionAnalyser(unit);
}

IRComSubExprElim::~IRComSubExprElim()
{
    delete pfa;
}

void IRComSubExprElim::insertLoadAfterStore()
{
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        for (auto bb = (*func)->begin(); bb != (*func)->end(); bb++)
        {
            for (auto inst = (*bb)->begin(); inst != (*bb)->end(); inst = inst->getNext())
            {
                if (inst->isStore())
                {
                    auto loadInst = new LoadInstruction(new Operand(new TemporarySymbolEntry(inst->getUse()[1]->getEntry()->getType(), SymbolTable::getLabel())), inst->getUse()[0], nullptr);
                    (*bb)->insertBefore(loadInst, inst->getNext());
                    inst = loadInst;
                    addedLoad.push_back(std::make_pair(loadInst, inst->getUse()[1]));
                }
            }
        }
    }
}

void IRComSubExprElim::removeLoadAfterStore()
{
    for (auto pa : addedLoad)
    {
        auto loadInst = pa.first;
        auto loadSrc = pa.second;
        auto allUseInst = std::vector<Instruction *>(loadInst->getDef()->getUse());
        for (auto inst : allUseInst)
        {
            inst->replaceUse(loadInst->getDef(), loadSrc);
        }
        loadInst->getParent()->remove(loadInst);
    }
}

void IRComSubExprElim::doCSE()
{
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        bool result1 = false, result2 = false;
        while (!result1 || !result2)
        {
            result1 = localCSE(*func);
            result2 = globalCSE(*func);
        }
    }
}

Instruction *IRComSubExprElim::preSameExpr(Instruction *inst)
{
    Instruction* preInst = nullptr;

    return preInst;
}

bool IRComSubExprElim::localCSE(Function *func)
{
    bool result = true;
    for (auto bb = func->begin(); bb != func->end(); bb++)
    {
        for (auto inst = (*bb)->begin(); inst != (*bb)->end(); inst = inst->getNext())
        {

        }
    }
    return result;
}

bool IRComSubExprElim::globalCSE(Function *)
{
    bool result = true;
    return result;
}

void IRComSubExprElim::pass()
{
    insertLoadAfterStore();
    doCSE();
    removeLoadAfterStore();
}
