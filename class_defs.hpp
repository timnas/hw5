#ifndef CLASS_DEFS_H
#define CLASS_DEFS_H
#include "hw3_output.hpp"
#include <list>
#include <vector>
#include <iostream>
#include "bp.hpp"
using namespace std;

/* classes */

class ASTNode;
#define YYSTYPE ASTNode*
class LabelM;
class ExitM;
class FuncDecl;
class OverRide;
class RetType;
class Formals;
class FormalDecl;
class Arg;
class FuncArgs;
class Expression;
class ExpList;
class Statements;
class Statement;
class OpenStatement;
class ClosedStatement;
class SomeStatement;
class Table;
class TableType;
class TableEntry;

/*  AST Nodes Declarations  */
class ASTNode
{
public:
    ASTNode(string value, int line_no);
    virtual ~ASTNode() = default;
    string value;
    int line_no;
    vector<pair<int, BranchLabelIndex>> nextlist;
    vector<pair<int, BranchLabelIndex>> continuelist;
    vector<pair<int, BranchLabelIndex>> breaklist;
};

class FuncDecl : public ASTNode
{
public:
    RetType *ret_type;
    string ret_type_str;
    bool isOverride;
    int num_args;
    vector<string> arg_types;
    FuncDecl(RetType *ret_type, ASTNode *node, Formals *func_args, ASTNode *is_override);
    FuncDecl(int line_no, string ret_type_str, bool is_override, int num_args, vector<string> *arg_types);
    void emit(ASTNode *node);
    void endFunc(Statements *statements);
};

//class Override : public FuncDecl {
//public:
//    Override(RetType* ret_type, ASTNode* node, Formals* params)
//            : FuncDecl(ret_type, node, params) {}
//};



class RetType : public ASTNode
{
public:
    RetType(ASTNode *node);
    RetType(string type, int line_no);
};

class Formals : public ASTNode
{
public:
    Formals(int line_no);
    Formals(FuncArgs* func_args);
    FuncArgs *func_args;
};


//class FormalDecl : public ASTNode {
//public:
//    string type;
//
//    FormalDecl(string type, ASTNode *node);
//    FormalDecl(FormalDecl* formal);
//    virtual ~FormalDecl(){}
//};

class Arg : public ASTNode
{
public:
    string arg_type;
    string arg_name;
    Arg() = default;
    Arg(ASTNode *type, ASTNode *id);
};

class FuncArgs : public ASTNode
{
public:
    list<Arg*> args_list;
    FuncArgs(int line_no);
    FuncArgs(Arg *arg);
    FuncArgs(Arg *arg, FuncArgs *func_args);
};

class Expression : public ASTNode
{
public:
    string type_name;
    string store_loc;
    bool is_val_calc;
    vector<pair<int, BranchLabelIndex>> truelist;
    vector<pair<int, BranchLabelIndex>> falselist;

    Expression(ASTNode *expression);
    Expression(ASTNode *node, string type_name);
    Expression(ASTNode *expression, ExpList *explist);
    Expression(ASTNode *node, string operation, Expression *expression);
    Expression(ASTNode *node, string type_name, string operation, Expression *exp1, Expression *exp2, LabelM *label_m);
    Expression(ASTNode *node, string type_name, string operation, Expression *exp1, Expression *exp2);
    void val_calc() {
        this->is_val_calc = true;
    }
};

class ExpList : public ASTNode
{
public:
    vector<Expression*> exp_list;
    ExpList(Expression *expression);
    ExpList(Expression *expression, ExpList *exp_list);
};

class Statement : public ASTNode
{
public:
    Statement(ASTNode *statement);
};

class Statements : public ASTNode
{
public:
    Statements(Statements *statements, LabelM *label_m, Statement *statement);
    Statements(Statement *statement);
};

class OpenStatement : public ASTNode
{
public:
    OpenStatement(Expression *expression);
    OpenStatement(Expression *expression, LabelM* label_m, ASTNode* node);
    OpenStatement(Expression* expression, LabelM* label_m1, ASTNode* node1, ExitM* exit_m, LabelM* label_m2, ASTNode* node2);
    OpenStatement(Expression* expression, LabelM* label_m1, LabelM* label_m2, ASTNode* node);
    };

class ClosedStatement : public ASTNode
{
public:
    ClosedStatement(Expression* expression, LabelM* label_m1, ASTNode* node1, ExitM* exit_m, LabelM* label_m2, ASTNode* node2);
    ClosedStatement(Expression *expression);
    ClosedStatement(SomeStatement* statement);
    ClosedStatement(int line_no);
    ClosedStatement(Expression* expression, LabelM* label_m1, LabelM* label_m2, ASTNode* node);

};

class SomeStatement : public ASTNode
{
public:
    SomeStatement(ASTNode *type, ASTNode *id);                  //(15)
    SomeStatement(ASTNode *type, ASTNode *id, Expression *expression); //(16)
    SomeStatement(string str, Expression *expression);           // 17, 20
    SomeStatement(ASTNode *node);                            // (14, 18,19,24,25)
};

/* Markers */
class LabelM : public ASTNode{
public:
    LabelM();
   // ~LabelM();
    string label;
};

class ExitM : public ASTNode{
public:
    ExitM();
 //   ~ExitM();
    vector<pair<int, BranchLabelIndex>> nextlist;
};


/* TABLE, TABLE TYPE AND TABLE ENTRY
 * These classes are used to represent symbol table entries in the syntactic analyzer.
 * The Table class contains a vector of TableEntry objects, and each TableEntry has a name, type, and offset.
 * The TableType class represents the type information, including whether it is a function,
 * the variable type (for non-function types), the return type (for function types), and the argument types (for function types).
 */


class Table {
public:
    Table() : contains_while_loop(false), scope_reg("") {}
    vector<TableEntry> table_entries_vec;
    bool contains_while_loop; // should get new value in newWhileScope func
    string scope_reg;
};

class TableType {
public:
    TableType() : function_type(false) {} // TODO: make sure these constructors are valid
    TableType(bool function_type, string variable_type) : function_type(function_type), variable_type(variable_type) {}
    TableType(bool function_type, FuncDecl *func_decl) : function_type(function_type), func_decl(func_decl) {}
   // TableType(const TableType &type) : function_type(type.function_type), variable_type(type.variable_type), return_type(type.return_type), args_type(type.args_type) {}
    bool function_type;
    string variable_type;
    //Timna change:
//    string return_type;
//    vector<string> args_type;
    FuncDecl *func_decl;

};

class TableEntry {
public:
    TableEntry() : offset(0) {}
    TableEntry(string name, TableType type, int offset) : name(name), type(type), offset(offset) {}
    TableEntry(const TableEntry &table_entry) : name(table_entry.name), type(table_entry.type), offset(table_entry.offset) {}
    string name;
    TableType type;
    int offset = 0;
};

/* Register allocation code */
class RegisterManager
{
private:
    RegisterManager() : next_reg(0), next_string(0) {}
    RegisterManager(RegisterManager const&);
    int next_reg;
    int next_string;
public:
    static RegisterManager &registerAlloc() {
        static RegisterManager register_alloc;
        return register_alloc;
    }
    string getNewRegister(){
        return "%reg_" + to_string(next_reg++);
    }
    string getNewString(){
        return "@.str_" + to_string(next_string++);
    }
};


/*
 * AUXILIARIES
 */

void initLLVM();
string getBoolReg(Expression* expression, bool bool_size);
void validateMain();
void initGlobalScope();
void newScope();
void newWhileScope();
void isBool(int line_num);
void printScopesSymbols();
bool varIdTaken(string id_name, TableType *id_type = nullptr);
bool IDInGlobalScope();
void addVarToStack(string name, string type);
TableEntry getFunctionEntry (ASTNode *node,ExpList* explist, FuncArgs* funcargs, bool* found, bool *ambiguous);
string getDataTypeRepresentation(const string &dataType);


#endif //CLASS_DEFS_H
