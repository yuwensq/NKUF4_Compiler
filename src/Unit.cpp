#include "Unit.h"
#include "Type.h"
#include "AsmBuilder.h"
#include "MachineCode.h"
#include "debug.h"
#include <sstream>
extern FILE *yyout;

void Unit::insertFunc(Function *f)
{
    func_list.push_back(f);
    if(f->getSymPtr()){
        if (f->getSymPtr()->toStr() == "@main")
            main = f;        
    }
}

void Unit::removeFunc(Function *func)
{
    func_list.erase(std::find(func_list.begin(), func_list.end(), func));
}

void Unit::printInitValOfArray(ArrayType *type, double *initVal, int startPos) const
{
    Assert(initVal, "initVal为空\n");
    std::vector<int> dim = type->getIndexs();
    Type *baseType = type->getBaseType();
    if (dim.size() > 0)
    {
        int size = dim[0];
        int subLen = type->getSize() / TypeSystem::intType->getSize() / size; // 子数组长度
        fprintf(yyout, "%s [", type->toStr().c_str());
        dim.erase(dim.begin());
        for (int i = 0; i < size; i++)
        {
            printInitValOfArray(new ArrayType(dim, baseType), initVal, startPos + i * subLen);
            if (i != size - 1)
                fprintf(yyout, ", ");
        }
        fprintf(yyout, "]");
    }
    else
    { // 是一个i32
        if (type->getBaseType()->isInt())
            fprintf(yyout, "%s %d", type->toStr().c_str(), (int)initVal[startPos]);
        else
            fprintf(yyout, "%s %f", type->toStr().c_str(), initVal[startPos]);
    }
    delete type; // 以后要注意释放内存了
}

/***
 * 由于没有在语法分析的时候建立se2func这个map，这里在
 * 查询se2func的时候逐步填充其内容
*/
Function *Unit::se2Func(SymbolEntry *se)
{
    if (se2func.find(se) != se2func.end())
        goto END;
    for (auto func : func_list)
    {
        if (func->getSymPtr() == se)
        {
            se2func[se] = func;
            break;
        }
    }
END:
    return se2func[se];
}

void Unit::output() const
{
    // 先打印全局变量
    for (auto var : global_vars)
    {
        IdentifierSymbolEntry *se = (IdentifierSymbolEntry *)var;
        if (!se->getType()->isArray())
        {
            double value = se->getValue();
            if (se->getType()->isInt())
                fprintf(yyout, "%s = global %s %d, align 4\n", se->toStr().c_str(), se->getType()->toStr().c_str(), (int)value);
            else if (se->getType()->isFloat())
            {
                value = (float)value;
                uint64_t v = reinterpret_cast<uint64_t &>(value);
                fprintf(yyout, "%s = global %s 0x%lx, align 4\n", se->toStr().c_str(), se->getType()->toStr().c_str(), v);
            }
        }
        else
        {
            auto initValue = se->getArrayValue();
            if (initValue == nullptr)
            {
                fprintf(yyout, "%s = global %s zeroinitializer, align 4\n", se->toStr().c_str(), se->getType()->toStr().c_str());
            }
            else
            {
                fprintf(yyout, "%s = global ", se->toStr().c_str());
                printInitValOfArray(new ArrayType(*((ArrayType *)se->getType())), initValue, 0);
                fprintf(yyout, ", align 4\n");
            }
            // double *initValue = se->getArrayValue();
            // fprintf(yyout, "%s = global ", se->toStr().c_str());
            // printInitValOfArray(new ArrayType(*((ArrayType *)se->getType())), initValue, 0);
            // fprintf(yyout, ", align 4\n");
        }
    }
    // 再打印函数
    for (auto &func : func_list)
        func->output();
    // 打印declare
    for (auto se : declare_list)
    {
        std::string type = ((FunctionType *)se->getType())->toStr();
        std::string name = se->toStr();
        std::string retType = type.substr(0, type.find("("));
        std::string fparams = type.substr(type.find("("));
        fprintf(yyout, "declare %s %s%s\n", retType.c_str(), name.c_str(), fparams.c_str());
    }
}

void Unit::genMachineCode(MachineUnit *munit)
{
    AsmBuilder *builder = new AsmBuilder();
    builder->setUnit(munit);
    munit->setGlobalVars(this->global_vars);
    for (auto &func : func_list)
        func->genMachineCode(builder);
}

Unit::~Unit()
{
    for (auto &func : func_list)
        delete func;
}