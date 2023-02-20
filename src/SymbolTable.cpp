#include "SymbolTable.h"
#include "Operand.h"
#include "Type.h"
#include <iostream>
#include <utility>
#include <sstream>

SymbolEntry::SymbolEntry(Type *type, int kind)
{
    this->type = type;
    this->kind = kind;
}

bool SymbolEntry::setNext(SymbolEntry *se)
{
    SymbolEntry *s = this;
    long unsigned int cnt = ((FunctionType *)(se->getType()))->getParamsType().size();
    if (cnt == ((FunctionType *)(s->getType()))->getParamsType().size())
        return false;
    while (s->getNext())
    {
        if (cnt == ((FunctionType *)(s->getType()))->getParamsType().size())
            return false;
        s = s->getNext();
    }
    s->next = se;
    return true;
}

ConstantSymbolEntry::ConstantSymbolEntry(Type *type, double value) : SymbolEntry(type, SymbolEntry::CONSTANT)
{
    this->value = value;
}

ConstantSymbolEntry::ConstantSymbolEntry(Type *type) : SymbolEntry(type, SymbolEntry::CONSTANT)
{
}

std::string ConstantSymbolEntry::toStr()
{
    std::ostringstream buffer;
    if (type->isInt())
        buffer << (int)value;
    else if (type->isFloat())
    {
        // 浮点数麻烦，直接打印16进制得了
        /* 终于找到了
        AFAIK just printing a decimal float works. If you
really want a hexadecimal encoding, just reinterpret the
floating-point number as an integer and print in hexadecimal; an "LLVM
float" is just an IEEE float printed in hexadecimal.
        */
        double fv = (float)value;
        uint64_t v = reinterpret_cast<uint64_t &>(fv);
        buffer << "0x" << std::hex << v;
    }
    return buffer.str();
}

IdentifierSymbolEntry::IdentifierSymbolEntry(Type *type, std::string name, int scope, bool sysy, int argNum) : SymbolEntry(type, SymbolEntry::VARIABLE), name(name), sysy(sysy)
{
    this->scope = scope;
    this->arrayValue = nullptr;
    // 如果是param，留一个Operand作为参数
    if (scope == PARAM)
    {
        TemporarySymbolEntry *se = new TemporarySymbolEntry(type, SymbolTable::getLabel());
        se->setIsParam(true);
        se->setSelfArgNum(argNum);
        argAddr = new Operand(se);
    }
    addr = nullptr;
}

void IdentifierSymbolEntry::setValue(double value)
{
    if (this->getType()->isInt() && ((IntType *)this->getType())->isConst() ||
        this->getType()->isFloat() && ((FloatType *)this->getType())->isConst())
    {
        if (!initial)
        {
            if (this->getType()->isInt())
                this->value = (int)value;
            else
                this->value = value;
            initial = true;
        }
        else
        {
            exit(-1);
        }
    }
    else
    {
        if (this->getType()->isInt())
            this->value = (int)value;
        else
            this->value = value;
    }
}

std::string IdentifierSymbolEntry::toStr()
{
    return "@" + name;
}

TemporarySymbolEntry::TemporarySymbolEntry(Type *type, int label) : SymbolEntry(type, SymbolEntry::TEMPORARY)
{
    this->label = label;
}

std::string TemporarySymbolEntry::toStr()
{
    std::ostringstream buffer;
    buffer << "%t" << label;
    return buffer.str();
}

SymbolTable::SymbolTable()
{
    prev = nullptr;
    level = 0;
}

SymbolTable::SymbolTable(SymbolTable *prev)
{
    this->prev = prev;
    this->level = prev->level + 1;
}

/*
    Description: lookup the symbol entry of an identifier in the symbol table
    Parameters:
        name: identifier name
    Return: pointer to the symbol entry of the identifier

    hint:
    1. The symbol table is a stack. The top of the stack contains symbol entries in the current scope.
    2. Search the entry in the current symbol table at first.
    3. If it's not in the current table, search it in previous ones(along the 'prev' link).
    4. If you find the entry, return it.
    5. If you can't find it in all symbol tables, return nullptr.
*/
// local变量表示是否只在当前作用域找
SymbolEntry *SymbolTable::lookup(std::string name, bool local)
{
    SymbolTable *table = this;
    while (table != nullptr)
    {
        if (table->symbolTable.find(name) != table->symbolTable.end())
        {
            return table->symbolTable[name];
        }
        table = table->prev;
        if (local)
        {
            if (table == nullptr || table->getLevel() != 1)
                break;
        }
    }
    return nullptr;
}

// install the entry into current symbol table.
void SymbolTable::install(std::string name, SymbolEntry *entry)
{
    // 如果是函数，要检查重定义
    if (entry->getType()->isFunc())
    {
        if (this->symbolTable.find(name) != this->symbolTable.end())
        {
            SymbolEntry *se = this->symbolTable[name];
            if (se->getType()->isFunc())
            {
                se->setNext(entry);
                return;
            }
        }
    }
    symbolTable[name] = entry;
}

int SymbolTable::counter = 0;
static SymbolTable t;
SymbolTable *identifiers = &t;
SymbolTable *globals = &t;
