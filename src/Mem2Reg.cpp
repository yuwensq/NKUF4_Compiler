#include "Mem2Reg.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include "BasicBlock.h"
#include "Instruction.h"
#include "ParamHandler.h"
using namespace std;

void Mem2Reg::pass()
{
    for (auto it = unit->begin(); it != unit->end(); it++)
    {
        checkCondBranch(*it);
        (*it)->computeDFSTree();
        (*it)->computeSdom();
        (*it)->computeIdom();
        (*it)->computeDomFrontier();
        insertPhiInstruction(*it);
        rename(*it);
        // 这个会导致r0-r3被覆盖
        cleanAddZeroIns(*it);
        checkPhiNodes(*it);
    }
    auto ph = new ParamHandler(unit);
    ph->pass();
}

void Mem2Reg::insertPhiInstruction(Function *function)
{
    addZeroIns.clear();
    allocaIns.clear();
    phiNodes.clear();
    vector<AllocaInstruction *> allocaArr;
    vector<Instruction *> uselessIns;

    BasicBlock *entry = function->getEntry();
    for (auto i = entry->begin(); i != entry->end(); i = i->getNext())
    {
        if (!i->isAlloc())
            break;
        auto alloca = (AllocaInstruction *)i;
        if (!alloca->getEntry()->getType()->isArray() && !alloca->getEntry()->getType()->isPtr())
            allocaIns.push_back(alloca);
        else
        {
            // Log("%s", alloca->getDef()->toStr().c_str());
            auto &v = alloca->getDef()->getUse();
            if (!v.empty())
            {
                if (v[0]->isStore())
                    allocaArr.push_back(alloca);
            }
            else
            {
                // only alloca, no use
                uselessIns.push_back(alloca);
            }
        }
    }
    queue<BasicBlock *> worklist;
    unordered_set<BasicBlock *> inWorklist, inserted, assigns;
    for (auto &i : allocaIns)
    {
        queue<BasicBlock *>().swap(worklist);
        inWorklist.clear();
        inserted.clear();
        assigns.clear();
        auto block = i->getParent();
        block->remove(i);
        auto operand = i->getDef();
        operand->setDef(nullptr);
        Operand *newOperand = new Operand(new TemporarySymbolEntry(
            ((PointerType *)(operand->getType()))->getType(),
            SymbolTable::getLabel()));
        i->replaceDef(newOperand);
        // Assume that an operand has and only has one def
        while (operand->use_begin() != operand->use_end())
        {
            auto use = operand->use_begin();
            if ((*use)->isStore())
            {
                if (newOperand != (*use)->getUse()[1])
                {
                    auto assignIns = new BinaryInstruction(
                        BinaryInstruction::ADD, newOperand, (*use)->getUse()[1],
                        new Operand(
                            new ConstantSymbolEntry(newOperand->getType(), 0)));
                    addZeroIns.push_back(assignIns);
                    (*use)->getParent()->insertBefore(assignIns, *use);
                    assigns.insert((*use)->getParent());
                    (*use)->getUse()[1]->removeUse(*use);
                }
            }
            auto dst = (*use)->getDef();
            (*use)->getParent()->remove(*use);
            if (dst && dst != newOperand)
                while (dst->use_begin() != dst->use_end())
                {
                    auto u = *(dst->use_begin());
                    // if (lastDef) {
                    //     u->replaceUse(dst, lastDef);
                    //     lastDef = nullptr;
                    // } else
                    u->replaceUse(dst, newOperand);
                }
            operand->removeUse(*use);
        }
        for (auto &block : assigns)
        {
            worklist.push(block);
            inWorklist.insert(block);
            while (!worklist.empty())
            {
                BasicBlock *n = worklist.front();
                worklist.pop();
                for (auto m : n->domFrontier)
                {
                    if (inserted.find(m) == inserted.end())
                    {
                        auto phi = new PhiInstruction(newOperand);
                        phiNodes.push_back(phi);
                        m->insertFront(phi, false);
                        inserted.insert(m);
                        if (inWorklist.find(m) == inWorklist.end())
                        {
                            inWorklist.insert(m);
                            worklist.push(m);
                        }
                    }
                }
            }
        }
    }
    for (auto &alloca : allocaArr)
    {
        auto defArray = alloca->getDef();
        auto storeArray = alloca->getDef()->getUse()[0]; // parameter => defArray
        if (storeArray->getUse()[0]->getUse().size() == 1)
        {
            // no use after alloca & store
            uselessIns.push_back(storeArray);
            uselessIns.push_back(alloca);
            continue;
        }
        auto paramArray = storeArray->getUse()[1];
        assert(paramArray->getEntry()->isTemporary());
        assert(((TemporarySymbolEntry *)paramArray->getEntry())->isParam());
        auto bitcast = new BitcastInstruction(defArray, paramArray); // defArray <= parameter
        entry->insertBefore(bitcast, storeArray);                    // alloca->bitcast->store
        entry->remove(storeArray);
        delete storeArray; // parameter->removeUse, defArray->removeUse
        entry->remove(alloca);
        delete alloca; // defArray->setDef(NULL)
        defArray->setDef(bitcast);
        auto uses = defArray->getUse(); // make a copy
        for (auto &&load_inst : uses)
        {
            // %t <= defArray
            if (load_inst == bitcast)
                continue;
            assert(load_inst->isLoad());
            if (!load_inst->isLoad())
                continue;
            auto use_def = load_inst->getDef();
            for (auto &&i : use_def->getUse())
            {
                i->replaceUse(use_def, defArray);
            }
            load_inst->getParent()->remove(load_inst);
            delete load_inst; // %t->setDef(NULL), %t will be deleted;  defArray->removeUse;
        }
        bitcast->getDef()->getEntry()->setType(paramArray->getType());
    }
    for (auto it = uselessIns.begin(); it != uselessIns.end(); it++)
    {
        entry->remove(*it);
        delete *it;
    }
}

void Mem2Reg::rename(Function *function)
{
    stacks.clear();
    for (auto &i : allocaIns)
    {
        auto operand = i->getDef();
        stacks[operand] = stack<Operand *>();
        // delete i;
    }
    rename(function->getEntry());
}

void Mem2Reg::rename(BasicBlock *block)
{
    std::unordered_map<Operand *, int> counter;
    for (auto i = block->begin(); i != block->end(); i = i->getNext())
    {
        Operand *def = i->getDef();
        if (def && stacks.find(def) != stacks.end())
        {
            counter[def]++;
            Operand *new_ = newName(def);
            i->replaceDef(new_);
        }
        if (!i->isPhi())
            for (auto &u : i->getUse())
                if (stacks.find(u) != stacks.end() && !stacks[u].empty())
                    i->replaceUse(u, stacks[u].top());
    }
    for (auto it = block->succ_begin(); it != block->succ_end(); it++)
    {
        for (auto i = (*it)->begin(); i != (*it)->end(); i = i->getNext())
        {
            if (!i->isPhi())
                break;
            auto phi = (PhiInstruction *)i;
            Operand *o = phi->getAddr();
            if (!stacks[o].empty())
            {
                phi->addEdge(block, stacks[o].top());
            }
            else
            {
                phi->addEdge(block, new Operand(new ConstantSymbolEntry(o->getType(), 0)));
            }
        }
    }
    auto func = block->getParent();
    auto node = func->getDomNode(block);
    for (auto &child : node->children)
        rename(child->block);
    for (auto &it : counter)
        for (int i = 0; i < it.second; i++)
            stacks[it.first].pop();
}

Operand *Mem2Reg::newName(Operand *old)
{
    Operand *new_name = old->getEntry()->isTemporary()
                            ? new Operand(new TemporarySymbolEntry(old->getEntry()->getType(), SymbolTable::getLabel()))
                            : new Operand(*old);
    stacks[old].push(new_name);
    return new_name;
}

void Mem2Reg::cleanAddZeroIns(Function *func)
{
    auto type = (FunctionType *)(func->getSymPtr()->getType());
    int paramNo = type->getParamsType().size() - 1;
    int regNum = 4;
    if (paramNo > 3)
        regNum--;
    for (auto i : addZeroIns)
    {
        auto use = i->getUse()[0];
        // if (use->getEntry()->isConstant())
        //     continue;
        if (i->getParent()->begin() == i && i->getNext()->isUncond())
            continue;
        if (use->getEntry()->isVariable())
        {
            continue;
            // if (func->hasCall())
            //     if (paramNo < regNum) {
            //         paramNo--;
            //         continue;
            //     }
            // if (paramNo >= regNum) {
            //     paramNo--;
            //     continue;
            // }
            // paramNo--;
        }
        auto def = i->getDef();
        // if (def != use)
        while (def->use_begin() != def->use_end())
        {
            auto u = *(def->use_begin());
            u->replaceUse(def, use);
        }
        i->getParent()->remove(i);
        use->removeUse(i);
        delete i;
    }
}

void Mem2Reg::checkCondBranch(Function *func)
{
    for (auto &block : func->getBlockList())
    {
        auto in = block->rbegin();
        if (in->isCond())
        {
            auto cond = (CondBrInstruction *)in;
            auto trueBlock = cond->getTrueBranch();
            auto falseBlock = cond->getFalseBranch();
            if (trueBlock == falseBlock)
            {
                block->removeSucc(trueBlock);
                trueBlock->removePred(block);
                new UncondBrInstruction(trueBlock, block);
                block->remove(in);
            }
        }
    }
}

void Mem2Reg::checkPhiNodes(Function *function)
{
    for (auto it = phiNodes.begin(); it != phiNodes.end(); it++)
    {
        auto &phi = *it;
        if (phi->getDef()->getUse().empty())
        {
            auto &srcs = phi->getOperands();
            for (size_t i = 1; i < srcs.size(); i++)
            {
                srcs[i]->removeUse(phi);
            }
            phi->getParent()->remove(phi);
            delete phi;
        }
    }
}
