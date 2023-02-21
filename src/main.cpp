#include <iostream>
#include <string.h>
#include <unistd.h>
#include "Ast.h"
#include "Unit.h"
#include "MachineCode.h"
#include "LinearScan.h"
using namespace std;

Ast ast;
Unit unit;
MachineUnit mUnit;
extern FILE *yyin;
extern FILE *yyout;

int yyparse();

char outfile[256] = "a.out";
bool dump_tokens;
bool dump_ast;
bool dump_ir;
bool dump_asm;

void globalFunc()
{
    // 添加一个memset函数用来初始化数组
    // memset(int*, int, int);
    std::vector<Type *> vec;
    vec.push_back(new PointerType(new ArrayType({})));
    vec.push_back(TypeSystem::intType);
    vec.push_back(TypeSystem::intType);
    Type *funcType = new FunctionType(TypeSystem::voidType, vec);
    SymbolEntry *se = new IdentifierSymbolEntry(funcType, "memset", globals->getLevel(), true);
    globals->install("memset", se);
}

int main(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "Siato:")) != -1)
    {
        switch (opt)
        {
        case 'o':
            strcpy(outfile, optarg);
            break;
        case 'a':
            dump_ast = true;
            break;
        case 't':
            dump_tokens = true;
            break;
        case 'i':
            dump_ir = true;
            break;
        case 'S':
            dump_asm = true;
            break;
        default:
            fprintf(stderr, "Usage: %s [-o outfile] infile\n", argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }
    if (optind >= argc)
    {
        fprintf(stderr, "no input file\n");
        exit(EXIT_FAILURE);
    }
    if (!(yyin = fopen(argv[optind], "r")))
    {
        fprintf(stderr, "%s: No such file or directory\nno input file\n", argv[optind]);
        exit(EXIT_FAILURE);
    }
    if (!(yyout = fopen(outfile, "w")))
    {
        fprintf(stderr, "%s: fail to open output file\n", outfile);
        exit(EXIT_FAILURE);
    }
    // dump_tokens = true;
    globalFunc();
    yyparse();
    if (dump_ast)
        ast.output();
    ast.typeCheck();
    ast.genCode(&unit);
    if (dump_ir)
        unit.output();
    unit.genMachineCode(&mUnit);
    LinearScan linearScan(&mUnit);
    linearScan.allocateRegisters();
    if (dump_asm)
        mUnit.output();
    return 0;
}
