#include "MachineDeadCodeElim.h"
#include "LiveVariableAnalysis.h"
#define getHash(x) MachineUnit::getHash(x)

void MachineDeadCodeElim::pass()
{
    LiveVariableAnalysis lva;
    lva.pass(munit);
    std::map<int, bool> willBeUsed;
    std::vector<MachineInstruction *> deadInstructions;
    MachineOperand *r[4];
    MachineOperand *s[16];
    for (int i = 0; i < 4; i++)
        r[i] = new MachineOperand(MachineOperand::REG, i);
    for (int i = 0; i < 16; i++)
        s[i] = new MachineOperand(MachineOperand::REG, i, true);
    auto sp = new MachineOperand(MachineOperand::REG, 13);
    for (auto mfunc : munit->getFuncs())
    {
        for (auto mb : mfunc->getBlocks())
        {
            deadInstructions.clear();
            willBeUsed.clear();
            for (auto op : mb->live_out)
                willBeUsed[getHash(op)] = true;
            for (auto itMinst = mb->rbegin(); itMinst != mb->rend(); itMinst++)
            {
                auto minst = *itMinst;
                if (minst->isRet())
                {
                    willBeUsed[getHash(r[0])] = true;
                    willBeUsed[getHash(s[0])] = true;
                    willBeUsed[getHash(sp)] = true;
                }
                if (minst->getDef().size() > 0)
                {
                    if (!willBeUsed[getHash(minst->getDef()[0])])
                        deadInstructions.push_back(minst);
                    /* sp永远不注销吧 */
                    if (getHash(minst->getDef()[0]) != getHash(sp) && !minst->isCondMov())
                        willBeUsed[getHash(minst->getDef()[0])] = false;
                }
                for (auto use : minst->getUse())
                    willBeUsed[getHash(use)] = true;
                if (minst->isCall())
                {
                    for (int i = 0; i < 4; i++)
                        willBeUsed[getHash(r[i])] = true;
                    for (int i = 0; i < 16; i++)
                        willBeUsed[getHash(s[i])] = true;
                }
            }
            for (auto deadCode : deadInstructions)
                mb->eraseInst(deadCode);
        }
    }
    for (int i = 0; i < 4; i++)
        delete r[i];
    for (int i = 0; i < 16; i++)
        delete s[i];
    delete sp;
}