#ifndef __GRAPH_COLOR_H_
#define __GRAPH_COLOR_H_
#include "MachineCode.h"
#include <set>
#include <map>
#include <vector>
#include <unordered_set>

class GraphColor
{
private:
    struct Node // 这个表示图的一个节点
    {
        int color;
        bool spill;
        int disp; // displacement in stack
        std::set<MachineOperand *> defs;
        std::set<MachineOperand *> uses;
        bool fpu; // 是不是浮点寄存器，可以把浮点数的图和普通寄存器的图分开
        Node()
        {
            color = -1;
            spill = false;
            fpu = false;
            disp = 0;
            defs.clear();
            uses.clear();
        }
        Node(bool freg, MachineOperand *def)
        {
            Node();
            fpu = freg;
            defs.insert(def);
        }
    };
    MachineUnit *unit;
    MachineFunction *func;
    std::vector<Node> nodes;                      // 存总的节点集合
    std::map<int, std::unordered_set<int>> graph; // 存图
    std::map<MachineOperand *, int> var2Node;     // 将虚拟寄存器映射到节点
    std::vector<int> spillNodes;                  // 加这个变量也是希望能够快一点，不过最后好像差不多
    void clearData();
    void intersection(std::set<MachineOperand *> &, std::set<MachineOperand *> &, std::set<MachineOperand *> &);
    void calDRGenKill(std::map<MachineBlock *, std::set<MachineOperand *>> &, std::map<MachineBlock *, std::set<MachineOperand *>> &);
    void calDRInOut(std::map<MachineBlock *, std::set<MachineOperand *>> &, std::map<MachineBlock *, std::set<MachineOperand *>> &, std::map<MachineBlock *, std::set<MachineOperand *>> &, std::map<MachineBlock *, std::set<MachineOperand *>> &);
    int mergeTwoNodes(int, int);
    void genNodes();
    void genInterfereGraph();
    bool tryColoring();
    bool graphColorRegisterAlloc();
    void genSpillCode();
    void modifyCode();

public:
    GraphColor(MachineUnit *munit) : unit(munit){};
    void allocateRegisters();
};

#endif