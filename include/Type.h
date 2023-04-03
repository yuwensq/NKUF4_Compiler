#ifndef __TYPE_H__
#define __TYPE_H__
#include <vector>
#include <string>

class Type
{
private:
    int kind;

protected:
    enum
    {
        INT,
        BOOL,
        VOID,
        FUNC,
        PTR,
        ARRAY,
        FLOAT
    };

public:
    Type(int kind) : kind(kind) {}
    virtual ~Type() {}
    virtual std::string toStr() = 0;
    bool isInt() const { return kind == INT; };
    bool isBool() const { return kind == BOOL; };
    bool isVoid() const { return kind == VOID; };
    bool isFunc() const { return kind == FUNC; };
    bool isPtr() const { return kind == PTR; };
    bool isArray() const { return kind == ARRAY; };
    bool isFloat() const { return kind == FLOAT; };
    int getKind() const { return kind; }
    virtual int getSize() const { return 0; }
    virtual bool isConst() const {return false;}
    virtual bool isStr() const {return false;}
};

class IntType : public Type
{
private:
    int size;
    bool constant;

public:
    IntType(int size, bool constant = false) : Type(size == 1 ? Type::BOOL : Type::INT), size(size), constant(constant) {}
    std::string toStr();
    bool isConst() const { return constant; }
    int getSize() const { return size; }
};

class FloatType : public Type
{
private:
    int size;
    bool constant;

public:
    FloatType(int size, bool constant = false) : Type(Type::FLOAT), size(size), constant(constant){};
    std::string toStr();
    bool isConst() const { return constant; }
    int getSize() const { return size; }
};

class VoidType : public Type
{
public:
    VoidType() : Type(Type::VOID) {}
    std::string toStr();
};

class FunctionType : public Type
{
private:
    Type *returnType;
    std::vector<Type *> paramsType;

public:
    FunctionType(Type *returnType, std::vector<Type *> paramsType) : Type(Type::FUNC), returnType(returnType), paramsType(paramsType){};
    Type *getRetType() { return returnType; };
    void setParamsType(std::vector<Type *> paramsType)
    {
        this->paramsType = paramsType;
    };
    std::vector<Type *> getParamsType() { return paramsType; }
    std::string toStr();
    int getSize() const { return returnType->getSize(); }
};

class PointerType : public Type
{
private:
    Type *valueType;

public:
    PointerType(Type *valueType) : Type(Type::PTR)
    {
        this->valueType = valueType;
    };
    std::string toStr();
    Type *getType() const { return valueType; }
    int getSize() const { return 32; }
};

class TypeSystem
{
private:
    static IntType commonInt;
    static IntType commonBool;
    static FloatType commonFloat;
    static VoidType commonVoid;
    static IntType commonConstInt;
    static FloatType commonConstFloat;

public:
    static Type *intType;
    static Type *floatType;
    static Type *voidType;
    static Type *boolType;
    static Type *constIntType;
    static Type *constFloatType;
};

class ArrayType : public Type
{
private:
    // 数组的索引范围
    std::vector<int> indexs;
    Type *baseType;
    int size;

public:
    ArrayType(std::vector<int> indexs, Type *baseType = TypeSystem::intType) : Type(Type::ARRAY), indexs(indexs), baseType(baseType)
    {
        this->size = 1;
        for (auto index : indexs)
        {
            this->size *= index;
        }
        this->size *= 32;
    };
    std::string toStr();
    std::vector<int> getIndexs() { return indexs; }
    Type *getBaseType() { return baseType; }
    int getSize() const { return size; }
    bool isConst() const { return baseType->isConst(); }
};

class StringType : public Type {
private:
    std::string a;
    size_t len;

public:
    StringType(std::string a) : Type(7), a(a), len(a.length()-1) {}
    std::string toStr() {return "[" + std::to_string(len) + " x i8]";}
    bool isConstType() {return true;}
    size_t getLength() {return len;}
    std::string get() {return a;}
    bool isStr() const {return true;}
};

#endif
