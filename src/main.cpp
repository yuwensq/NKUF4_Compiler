#include <iostream>
#include <string.h>
#include <unistd.h>
#include "Ast.h"
#include "Unit.h"
#include "SymbolTable.h"
#include "MachineCode.h"
#include "LinearScan.h"
#include "MachinePeepHole.h"
#include "MachineStraight.h"
#include "Mem2Reg.h"
#include "SimplifyCFG.h"
#include "IRSparseCondConstProp.h"
#include "PureFunctionAnalyser.h"
#include "IRComSubExprElim.h"
#include "PhiElim.h"
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
    yyparse();
    Log("语法分析成功\n");
    if (dump_ast)
        ast.output();
    ast.typeCheck();
    ast.genCode(&unit);
    Log("中间代码生成成功\n");

    Mem2Reg m2r(&unit);
    IRSparseCondConstProp sccp(&unit);
    IRComSubExprElim cse(&unit);
    PhiElimination pe(&unit);

    m2r.pass(); //Only IR supported
    sccp.pass();
    cse.pass();
    sccp.pass();
    // pe.pass();

    
    Log("IR优化成功\n");/**/

    if (dump_ir)
        unit.output();
    unit.genMachineCode(&mUnit);
    Log("目标代码生成成功\n");

    MachinePeepHole mph(&mUnit, 2);
    MachineStraight mst(&mUnit);
    mst.pass();
    // mph.pass();

    Log("目标代码优化成功\n");

    LinearScan linearScan(&mUnit);
    if (dump_asm)
        linearScan.allocateRegisters();
    Log("线性扫描完成\n");
    if (dump_asm)
        mUnit.output();
    return 0;
}
