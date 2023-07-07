#include "GraphColor.h"
#include "debug.h"
#include <queue>
extern FILE *yyout;

void GraphColor::clearData()
{
    nodes.clear();
    graph.clear();
    var2Node.clear();
    spillNodes.clear();
}

void GraphColor::intersection(std::set<MachineOperand *> &a, std::set<MachineOperand *> &b, std::set<MachineOperand *> &out)
{
    out.clear();
    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), inserter(out, out.begin()));
}

void GraphColor::calDRGenKill(std::map<MachineBlock *, std::set<MachineOperand *>> &gen, std::map<MachineBlock *, std::set<MachineOperand *>> &kill)
{
    for (auto &block : func->getBlocks())
    {
        for (auto &inst : block->getInsts())
        {
            if (inst->getDef().size() > 0)
            {
                assert(inst->getDef().size() == 1);
                // 学学新用法
                auto def = inst->getDef()[0];
                nodes.emplace_back(def->isFReg(), def);
                var2Node[def] = nodes.size() - 1;
                auto copyGen = gen[block];
                for (auto op : copyGen)
                    if ((*op) == (*def))
                        gen[block].erase(op);
                kill[block].insert(def);
            }
        }
    }
}

void GraphColor::calDRInOut(std::map<MachineBlock *, std::set<MachineOperand *>> &gen, std::map<MachineBlock *, std::set<MachineOperand *>> &kill, std::map<MachineBlock *, std::set<MachineOperand *>> &in, std::map<MachineBlock *, std::set<MachineOperand *>> &out)
{
    auto entry = func->getEntry();
    out[entry] = gen[entry];
    std::queue<MachineBlock *> workList;
    for (auto block : func->getBlocks())
        if (block != entry)
            workList.push(block);
    while (!workList.empty())
    {
        auto block = workList.front();
        workList.pop();
        std::set<MachineOperand *> In[2];
        In[0] = out[block->getPreds()[0]];
        auto it = block->getPreds().begin() + 1;
        auto overPos = block->getPreds().end();
        int turn = 0;
        for (; it != overPos; it++)
        {
            intersection(out[*it], In[turn], In[turn ^ 1]);
            turn ^= 1;
        }
        in[block] = In[turn];
        // 计算out
        std::set<MachineOperand *> Out;
        for (auto indef : in[block])
            for (auto def : kill[block])
                if (*indef == *def)
                    In[turn].erase(indef);
        std::set_union(gen[block].begin(), gen[block].end(), In[turn].begin(), In[turn].end(), inserter(Out, Out.begin()));
        if (out[block] != Out)
        {
            out[block] = Out;
            for (auto &succ : block->getSuccs())
                workList.push(succ);
        }
    }
}

int GraphColor::mergeTwoNodes(int no1, int no2)
{
    int dst = std::min(no1, no2);
    int src = std::max(no1, no2);
    assert(dst != src && nodes[src].fpu == nodes[dst].fpu);
    nodes[dst].defs.insert(nodes[src].defs.begin(), nodes[src].defs.end());
    nodes[dst].uses.insert(nodes[src].uses.begin(), nodes[src].uses.end());
    for (auto &def : nodes[dst].defs)
        var2Node[def] = dst;
    for (auto &use : nodes[dst].uses)
        var2Node[use] = dst;
    return dst;
}

void GraphColor::genNodes()
{
    std::map<MachineBlock *, std::set<MachineOperand *>> gen;
    std::map<MachineBlock *, std::set<MachineOperand *>> kill;
    std::map<MachineBlock *, std::set<MachineOperand *>> in;
    std::map<MachineBlock *, std::set<MachineOperand *>> out;
    calDRGenKill(gen, kill);
    calDRInOut(gen, kill, in, out);
    // 这个主要就是一个运算数可能有多个def都可以到达他
    std::map<MachineOperand, std::set<MachineOperand *>> op2Def;
    for (auto &block : func->getBlocks())
    {
        op2Def.clear();
        for (auto &def : in[block])
            op2Def[*def].insert(def);
        for (auto &inst : block->getInsts())
        {
            for (auto &use : inst->getUse())
            {
                if (!use->isVReg())
                    continue;
                // 这里如果小于等于零，表示当前use的定值不存在，不太可能
                // assert(op2Def[*use].size() > 0);
                if (op2Def[*use].size() <= 0) {
                    inst->output();
                    Log("x");
                    exit(0);
                }
                auto it = op2Def[*use].begin();
                auto overPos = op2Def[*use].end();
                int no1 = var2Node[*it];
                it++;
                for (; it != overPos; it++)
                {
                    int no2 = var2Node[*it];
                    no1 = mergeTwoNodes(no1, no2);
                }
                nodes[no1].uses.insert(use);
                var2Node[use] = no1;
            }
            if (inst->getDef().size() > 0)
            {
                auto def = inst->getDef()[0];
                op2Def.erase(*def);
                op2Def[*def].insert(def);
            }
        }
    }
}

void GraphColor::genInterfereGraph()
{
}

bool GraphColor::tryColoring()
{
    return false;
}

bool GraphColor::graphColorRegisterAlloc()
{
    clearData();
    genNodes();
    genInterfereGraph();
    // coalescing();
    return tryColoring();
}

void GraphColor::genSpillCode()
{
    for (auto nodeNo : spillNodes)
    {
        auto node = nodes[nodeNo];
        if (!node.spill)
            continue;
        // 在栈中分配内存
        node.disp = func->AllocSpace(4) * -1;
        // 在use前插入ldr
        for (auto use : node.uses)
        {
            auto cur_bb = use->getParent()->getParent();
            MachineInstruction *cur_inst = nullptr;
            if (node.fpu)
            {
                if (node.disp >= -1020)
                {
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::VLDR,
                                                    new MachineOperand(*use),
                                                    new MachineOperand(MachineOperand::REG, 11),
                                                    new MachineOperand(MachineOperand::IMM, node.disp));
                    cur_bb->insertBefore(cur_inst, use->getParent());
                }
                else
                {
                    auto internal_reg = new MachineOperand(MachineOperand::VREG, SymbolTable::getLabel());
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::LDR,
                                                    internal_reg,
                                                    new MachineOperand(MachineOperand::IMM, node.disp));
                    cur_bb->insertBefore(cur_inst, use->getParent());
                    cur_inst = new BinaryMInstruction(cur_bb,
                                                      BinaryMInstruction::ADD,
                                                      new MachineOperand(*internal_reg),
                                                      new MachineOperand(*internal_reg),
                                                      new MachineOperand(MachineOperand::REG, 11));
                    cur_bb->insertBefore(cur_inst, use->getParent());
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::VLDR,
                                                    new MachineOperand(*use),
                                                    new MachineOperand(*internal_reg));
                    cur_bb->insertBefore(cur_inst, use->getParent());
                }
            }
            else
            {
                // https://developer.arm.com/documentation/dui0473/m/arm-and-thumb-instructions/ldr--immediate-offset-?lang=en
                if (node.disp >= -4095)
                {
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::LDR,
                                                    new MachineOperand(*use),
                                                    new MachineOperand(MachineOperand::REG, 11),
                                                    new MachineOperand(MachineOperand::IMM, node.disp));
                    cur_bb->insertBefore(cur_inst, use->getParent());
                }
                else
                {
                    auto internal_reg = new MachineOperand(MachineOperand::VREG, SymbolTable::getLabel());
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::LDR,
                                                    internal_reg,
                                                    new MachineOperand(MachineOperand::IMM, node.disp));
                    cur_bb->insertBefore(cur_inst, use->getParent());
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::LDR,
                                                    new MachineOperand(*use),
                                                    new MachineOperand(MachineOperand::REG, 11),
                                                    new MachineOperand(*internal_reg));
                    cur_bb->insertBefore(cur_inst, use->getParent());
                }
            }
        }

        // 在def后插入str
        for (auto def : node.defs)
        {
            auto cur_bb = def->getParent()->getParent();
            MachineInstruction *cur_inst = nullptr;
            if (node.fpu)
            {
                if (node.disp >= -1020)
                {
                    cur_inst = new StoreMInstruction(cur_bb,
                                                     StoreMInstruction::VSTR,
                                                     new MachineOperand(*def),
                                                     new MachineOperand(MachineOperand::REG, 11),
                                                     new MachineOperand(MachineOperand::IMM, node.disp));
                    cur_bb->insertAfter(cur_inst, def->getParent());
                }
                else
                {
                    auto internal_reg = new MachineOperand(MachineOperand::VREG, SymbolTable::getLabel());
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::LDR,
                                                    internal_reg,
                                                    new MachineOperand(MachineOperand::IMM, node.disp));
                    cur_bb->insertAfter(cur_inst, def->getParent());
                    auto cur_inst1 = new BinaryMInstruction(cur_bb,
                                                            BinaryMInstruction::ADD,
                                                            new MachineOperand(*internal_reg),
                                                            new MachineOperand(*internal_reg),
                                                            new MachineOperand(MachineOperand::REG, 11));
                    cur_bb->insertAfter(cur_inst1, cur_inst);
                    auto cur_inst2 = new StoreMInstruction(cur_bb,
                                                           StoreMInstruction::VSTR,
                                                           new MachineOperand(*def),
                                                           new MachineOperand(*internal_reg));

                    cur_bb->insertAfter(cur_inst2, cur_inst1);
                }
            }
            else
            {
                // https://developer.arm.com/documentation/dui0473/m/arm-and-thumb-instructions/ldr--immediate-offset-?lang=en
                if (node.disp >= -4095)
                {
                    cur_inst = new StoreMInstruction(cur_bb,
                                                     StoreMInstruction::STR,
                                                     new MachineOperand(*def),
                                                     new MachineOperand(MachineOperand::REG, 11),
                                                     new MachineOperand(MachineOperand::IMM, node.disp));
                    cur_bb->insertAfter(cur_inst, def->getParent());
                }
                else
                {
                    auto internal_reg = new MachineOperand(MachineOperand::VREG, SymbolTable::getLabel());
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::LDR,
                                                    internal_reg,
                                                    new MachineOperand(MachineOperand::IMM, node.disp));
                    cur_bb->insertAfter(cur_inst, def->getParent());
                    auto cur_inst1 = new StoreMInstruction(cur_bb,
                                                           StoreMInstruction::STR,
                                                           new MachineOperand(*def),
                                                           new MachineOperand(MachineOperand::REG, 11),
                                                           new MachineOperand(*internal_reg));
                    cur_bb->insertAfter(cur_inst1, cur_inst);
                }
            }
        }
    }
}

void GraphColor::modifyCode()
{
    for (auto &node : nodes)
    {
        if (node.color == -1)
            continue;
        func->addSavedRegs(node.color);
        for (auto def : node.defs)
            def->setReg(node.color);
        for (auto use : node.uses)
            use->setReg(node.color);
    }
}

void GraphColor::allocateRegisters()
{
    int counter = 0;
    for (auto &f : unit->getFuncs())
    {
        func = f;
        bool success = false;
        if (counter >= 1000)
            break;
        while (!success)
        {
            success = graphColorRegisterAlloc();
            counter++;
            Log("counter %d", counter);
            if (!success)
                genSpillCode();
            else
                modifyCode();
        }
    }
}
