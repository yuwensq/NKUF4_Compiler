%option noyywrap
%{
    #define YY_NO_UNPUT
    #define YY_NO_INPUT
    #include "parser.h"
    #include <ostream>
    #include <fstream>
    using namespace std;
    extern FILE *yyin; 
    extern FILE *yyout;
    extern bool dump_tokens;
    int chars = 0;
    void DEBUG_FOR_LAB4(std::string s){
        std::string DEBUG_INFO = "[DEBUG LAB4]: \t" + s + "\n";
        fputs(DEBUG_INFO.c_str(), yyout);
    }
    #include <iostream>
%}
DECIMAL_FLOAT ((([0-9]*[.][0-9]*([eE][+-]?[0-9]+)?)|([0-9]+[eE][+-]?[0-9]+))[fLlL]?)
HEXADECIMAL_FLOAT (0[xX](([0-9A-Fa-f]*[.][0-9A-Fa-f]*([pP][+-]?[0-9]+)?)|([0-9A-Fa-f]+[pP][+-]?[0-9]+))[fLlL]?)
DECIMIAL ([1-9][0-9]*|0)
OCTAL (0[0-7]+)
HEXADECIMAL (0[xX][0-9A-Fa-f]+)
ID [[:alpha:]_][[:alpha:][:digit:]_]*
EOL (\r\n|\n|\r)
WHITE [\t ]
BLOCKCOMMENTBEGIN \/\*
BLOCKCOMMENTELEMENT .
BLOCKCOMMENTEND \*\/
%x BLOCKCOMMENT
LINECOMMENT \/\/[^\n]*
%%
"int" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("INT\tint");
    chars += strlen("int");
    return INT;
}
"float" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("FLOAT\tfloat");
    chars += strlen("float");
    return FLOAT;
}
"void" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("VOID\tvoid");
    chars += strlen("void");
    return VOID;
}
"const" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("CONST\tconst");
    chars += strlen("const");
    return CONST;
}
"if" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("IF\tif");
    chars += strlen("if");
    return IF;
};
"else" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("ELSE\telse");
    chars += strlen("else");
    return ELSE;
};
"return" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("RETURN\treturn");
    chars += strlen("return");
    return RETURN;
}
"while" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("WHILE\twhile");
    chars += strlen("while");
    return WHILE;
}
"break" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("BREAK\tbreak");
    chars += strlen("break");
    return BREAK;
}
"continue" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("CONTINUE\tcontinue");
    chars += strlen("continue");
    return CONTINUE;
}
"==" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("EQUAL\t==");
    chars += strlen("==");
    return EQUAL;
}
"!=" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("NOTEQUAL\t!=");
    chars += strlen("!=");
    return NOTEQUAL;
}
"=" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("ASSIGN\t=");
    chars += strlen("=");
    return ASSIGN;
}
"<" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("LESS\t<");
    chars += strlen("<");
    return LESS;
}
"<=" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("LESSEQUAL\t<");
    chars += strlen("<=");
    return LESSEQUAL;
}
">" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("GREATER\t<");
    chars += strlen(">");
    return GREATER;
}
">=" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("GREATEREQUAL\t<");
    chars += strlen(">=");
    return GREATEREQUAL;
}
"+" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("ADD\t+");
    chars += strlen("+");
    return ADD;
}
"-" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("SUB\t-");
    chars += strlen("-");
    return SUB;
}
"*" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("MUL\t*");
    chars += strlen("*");
    return MUL;
}
"/" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("DIV\t/");
    chars += strlen("/");
    return DIV;
}
"%" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("MOD\t%");
    chars += strlen("%");
    return MOD;
}
"&&" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("AND\t&&");
    chars += strlen("&&");
    return AND;
}
"||" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("OR\t||");
    chars += strlen("||");
    return OR;
}
"!" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("NOT\t!");
    chars += strlen("!");
    return NOT;
}
";" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("SEMICOLON\t;");
    chars += strlen(";");
    return SEMICOLON;
}
"(" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("LPAREN\t(");
    chars += strlen("(");
    return LPAREN;
}
")" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("RPAREN\t)");
    chars += strlen(")");
    return RPAREN;
}
"{" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("LBRACE\t{");
    chars += strlen("{");
    return LBRACE;
}
"}" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("RBRACE\t}");
    chars += strlen("}");
    return RBRACE;
}
"[" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("LBRACKET\t[");
    chars += strlen("[");
    return LBRACKET;
}
"]" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("RBRACKET\t}");
    chars += strlen("]");
    return RBRACKET;
}
"," {
    if(dump_tokens)
        DEBUG_FOR_LAB4("COMMA\t,");
    chars += strlen(",");
    return COMMA;
}
"putint" {
    if(dump_tokens)
        DEBUG_FOR_LAB4(yytext);
    char *lexeme;
    lexeme = new char[strlen(yytext) + 1];
    strcpy(lexeme, yytext);
    yylval.strtype = lexeme;
    std::vector<Type*> vec;
    vec.push_back(TypeSystem::intType);
    Type* funcType = new FunctionType(TypeSystem::voidType, vec);
    SymbolEntry* se = new IdentifierSymbolEntry(funcType, yytext, globals->getLevel(), true);
    globals->install(yytext, se);
    return ID;
}
"getint" {
    if(dump_tokens)
        DEBUG_FOR_LAB4(yytext);
    char *lexeme;
    lexeme = new char[strlen(yytext) + 1];
    strcpy(lexeme, yytext);
    yylval.strtype = lexeme;
    std::vector<Type*> vec;
    Type* funcType = new FunctionType(TypeSystem::intType, vec);
    SymbolEntry* se = new IdentifierSymbolEntry(funcType, yytext, globals->getLevel(), true);
    globals->install(yytext, se);
    return ID;
}
"putfloat" {
    if(dump_tokens)
        DEBUG_FOR_LAB4(yytext);
    char *lexeme;
    lexeme = new char[strlen(yytext) + 1];
    strcpy(lexeme, yytext);
    yylval.strtype = lexeme;
    Type* funcType = new FunctionType(TypeSystem::voidType, {TypeSystem::floatType});
    SymbolEntry* se = new IdentifierSymbolEntry(funcType, yytext, globals->getLevel(), true);
    globals->install(yytext, se);
    return ID;
}
"getfloat" {
    if(dump_tokens)
        DEBUG_FOR_LAB4(yytext);
    char *lexeme;
    lexeme = new char[strlen(yytext) + 1];
    strcpy(lexeme, yytext);
    yylval.strtype = lexeme;
    Type* funcType = new FunctionType(TypeSystem::floatType, {});
    SymbolEntry* se = new IdentifierSymbolEntry(funcType, yytext, globals->getLevel(), true);
    globals->install(yytext, se);
    return ID;
}
"putch" {
    if(dump_tokens)
        DEBUG_FOR_LAB4(yytext);
    char *lexeme;
    lexeme = new char[strlen(yytext) + 1];
    strcpy(lexeme, yytext);
    yylval.strtype = lexeme;
    std::vector<Type*> vec;
    vec.push_back(TypeSystem::intType);
    Type* funcType = new FunctionType(TypeSystem::voidType, vec);
    SymbolEntry* se = new IdentifierSymbolEntry(funcType, yytext, globals->getLevel(), true);
    globals->install(yytext, se);
    return ID;
}
"getch" {
    if(dump_tokens)
        DEBUG_FOR_LAB4(yytext);
    char *lexeme;
    lexeme = new char[strlen(yytext) + 1];
    strcpy(lexeme, yytext);
    yylval.strtype = lexeme;
    std::vector<Type*> vec;
    Type* funcType = new FunctionType(TypeSystem::intType, vec);
    SymbolEntry* se = new IdentifierSymbolEntry(funcType, yytext, globals->getLevel(), true);
    globals->install(yytext, se);
    return ID;
}
"putarray" {
    if(dump_tokens)
        DEBUG_FOR_LAB4(yytext);
    char *lexeme;
    lexeme = new char[strlen(yytext) + 1];
    strcpy(lexeme, yytext);
    yylval.strtype = lexeme;
    std::vector<Type*> vec;
    vec.push_back(TypeSystem::intType);
    vec.push_back(new PointerType(new ArrayType({})));
    Type* funcType = new FunctionType(TypeSystem::voidType, vec);
    SymbolEntry* se = new IdentifierSymbolEntry(funcType, yytext, globals->getLevel(), true);
    globals->install(yytext, se);
    return ID;
}
"getarray" {
    if(dump_tokens)
        DEBUG_FOR_LAB4(yytext);
    char *lexeme;
    lexeme = new char[strlen(yytext) + 1];
    strcpy(lexeme, yytext);
    yylval.strtype = lexeme;
    Type* funcType = new FunctionType(TypeSystem::intType, {new PointerType(new ArrayType({}))});
    SymbolEntry* se = new IdentifierSymbolEntry(funcType, yytext, globals->getLevel(), true);
    globals->install(yytext, se);
    return ID;
}
"putfarray" {
    if(dump_tokens)
        DEBUG_FOR_LAB4(yytext);
    char *lexeme;
    lexeme = new char[strlen(yytext) + 1];
    strcpy(lexeme, yytext);
    yylval.strtype = lexeme;
    std::vector<Type*> vec;
    vec.push_back(TypeSystem::intType);
    vec.push_back(new PointerType(new ArrayType({}, TypeSystem::floatType)));
    Type* funcType = new FunctionType(TypeSystem::voidType, vec);
    SymbolEntry* se = new IdentifierSymbolEntry(funcType, yytext, globals->getLevel(), true);
    globals->install(yytext, se);
    return ID;
}
"getfarray" {
    if(dump_tokens)
        DEBUG_FOR_LAB4(yytext);
    char *lexeme;
    lexeme = new char[strlen(yytext) + 1];
    strcpy(lexeme, yytext);
    yylval.strtype = lexeme;
    Type* funcType = new FunctionType(TypeSystem::intType, {new PointerType(new ArrayType({}, TypeSystem::floatType))});
    SymbolEntry* se = new IdentifierSymbolEntry(funcType, yytext, globals->getLevel(), true);
    globals->install(yytext, se);
    return ID;
}
"starttime" {
    if(dump_tokens)
        DEBUG_FOR_LAB4(yytext);
    char *lexeme;
    lexeme = new char[7 + strlen(yytext)];
    char* sysy = (char *)"_sysy_";
    strcpy(lexeme, sysy);
    strcpy(lexeme + 6, yytext);
    yylval.strtype = lexeme;
    Type* funcType = new FunctionType(TypeSystem::voidType, {TypeSystem::intType});
    SymbolEntry* se = new IdentifierSymbolEntry(funcType, "_sysy_starttime", globals->getLevel(), true);
    globals->install("_sysy_starttime", se);
    return ID;
}
"stoptime" {
    if(dump_tokens)
        DEBUG_FOR_LAB4(yytext);
    char *lexeme;
    lexeme = new char[7 + strlen(yytext)];
    char* sysy = (char *)"_sysy_";
    strcpy(lexeme, sysy);
    strcpy(lexeme + 6, yytext);
    yylval.strtype = lexeme;
    Type* funcType = new FunctionType(TypeSystem::voidType, {TypeSystem::intType});
    SymbolEntry* se = new IdentifierSymbolEntry(funcType, "_sysy_stoptime", globals->getLevel(), true);
    globals->install("_sysy_stoptime", se);
    return ID;
}
{DECIMIAL} {
    if(dump_tokens)
        DEBUG_FOR_LAB4(string("NUMBER\t") + string(yytext));
    chars += strlen(yytext);
    yylval.inttype = double(atoi(yytext));
    return INTEGER;
}
{OCTAL} {
    int temp;
    sscanf(yytext, "%o", &temp);
    if(dump_tokens)
        DEBUG_FOR_LAB4(string("NUMBER\t") + to_string(temp));
    yylval.inttype = temp;
    return INTEGER;
}
{HEXADECIMAL} {
    int temp;
    sscanf(yytext, "%x", &temp);
    if(dump_tokens)
        DEBUG_FOR_LAB4(string("NUMBER\t") + to_string(temp));
    yylval.inttype = temp;
    return INTEGER;
}
{DECIMAL_FLOAT} {
    double temp;
    sscanf(yytext, "%lf", &temp);
    if (dump_tokens) 
        DEBUG_FOR_LAB4(string("FLOATNUMBER\t") + to_string(temp));
    yylval.floattype = temp;
    return FLOATNUM;
}
{HEXADECIMAL_FLOAT} {
    float temp;
    sscanf(yytext, "%f", &temp);
    if (dump_tokens) 
        DEBUG_FOR_LAB4(string("FLOATNUMBER\t") + to_string(temp));
    yylval.floattype = temp;
    return FLOATNUM;
}
{ID} {
    if(dump_tokens)
        DEBUG_FOR_LAB4(yytext);
    char *lexeme;
    lexeme = new char[strlen(yytext) + 1];
    strcpy(lexeme, yytext);
    lexeme[strlen(yytext)] = 0;
    yylval.strtype = lexeme;
    return ID;
}
<*>{EOL} {
    chars += strlen(yytext);
    yylineno++;
    chars = 0;
}
{WHITE}
{LINECOMMENT} {}
{BLOCKCOMMENTBEGIN} {BEGIN BLOCKCOMMENT;}
<BLOCKCOMMENT>{BLOCKCOMMENTELEMENT} {}
<BLOCKCOMMENT>{BLOCKCOMMENTEND} {BEGIN INITIAL;}

%%
