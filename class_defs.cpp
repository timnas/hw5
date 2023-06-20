#include "class_defs.hpp"
#include <iostream>
#include "bp.hpp"
using namespace std;

string g_return_type;
string g_function_name;
string g_exp_type;
vector<Table> tables_stack;
vector<int> offsets_stack;
const int BYTE_MAX_SIZE = 255;

/* ASTNode Implementation */

ASTNode::ASTNode(string value, int line_no) : value(value), line_no(line_no) {
 //   std::cout<< "DEBUG ASTNode" <<std::endl;
}

/* FuncDecl Implementation */
FuncDecl::FuncDecl(int line_no, string ret_type_str, bool is_override, int num_args, vector<string> *arg_types) :
        ASTNode("FuncDecl", line_no), ret_type(nullptr), ret_type_str(ret_type_str), isOverride(is_override), num_args(num_args), arg_types(*arg_types) {

}

FuncDecl::FuncDecl(RetType *ret_type, ASTNode *node, Formals *formals, ASTNode* is_override) :
        ASTNode("FuncDecl", node->line_no), ret_type(ret_type) {
    if (ret_type != nullptr) {
        ret_type_str = ret_type->value;
    }
    if (is_override->value == "override_flag_true") {
        isOverride = true;
    } else {
        isOverride = false;
    }
    arg_types = {};

    // Calculate the number of arguments and store their types
    if (formals->func_args != nullptr) {
        for (auto arg = formals->func_args->args_list.begin(); arg != formals->func_args->args_list.end(); arg++) {
            arg_types.push_back((*arg)->arg_type);
        }
    }

    if (isOverride && node->value == "main") {
        output::errorMainOverride(node->line_no);
        exit(0);
    }

    bool func_name_exists = false;
    Table global_table = tables_stack[0];
    for (TableEntry &entry : global_table.table_entries_vec)
    {
        if (entry.name.compare(node->value) == 0)
        {
            if (!isOverride && entry.type.func_decl->isOverride) {
                output::errorOverrideWithoutDeclaration(node->line_no, g_function_name);
                exit(0);
            }
        }
    }
//    bool ambiguous = false;
//    TableEntry check_if_override_wo_dec = getFunctionEntry(node, nullptr, nullptr, &func_name_exists, &ambiguous);
//    if (func_name_exists) {
//      //  std::cout<<"first if"<<std::endl;
//        if (!isOverride && check_if_override_wo_dec.type.func_decl->isOverride) { //first dunc is not override
//             //  std::cout<<"second if"<<std::endl;
//            output::errorOverrideWithoutDeclaration(node->line_no, g_function_name);
//            exit(0);
//        }
//    }

    func_name_exists = false;
    bool ambiguous = false;
    TableEntry override_func = getFunctionEntry(node, nullptr, formals->func_args, &func_name_exists, &ambiguous);
    if (func_name_exists) {
//        std::cout<<"first if"<<std::endl;
        if (isOverride && !override_func.type.func_decl->isOverride) {
//            std::cout<<"second if"<<std::endl;
            output::errorFuncNoOverride(node->line_no, g_function_name);
            exit(0);
        } else if ((isOverride && override_func.type.func_decl->isOverride
                   && arg_types.size() == override_func.type.func_decl->arg_types.size()
                   && arg_types == override_func.type.func_decl->arg_types
                   && ret_type_str == override_func.type.func_decl->ret_type_str)
                   || ( !isOverride && !override_func.type.func_decl->isOverride)) { // both aren't override
//            std::cout<<"overriding func's num_args: "<< override_func.type.func_decl->arg_types.size() <<std::endl;
//            std::cout<<"overriding func's arg types: "<<arg_types.at(1)<<std::endl;
//            std::cout<<"overriden func's arg types: "<<override_func.type.func_decl->arg_types.at(1)<<std::endl;
//            std::cout<<"erroDef 5"<<std::endl;
            output::errorDef(node->line_no, g_function_name);
            exit(0);
        }
    }
    TableType func_type(true, this);
//    std::cout <<"arg types size: " <<this->arg_types.size() <<std::endl;
    TableEntry func_entry(g_function_name, func_type, 0);
    tables_stack[0].table_entries_vec.push_back(func_entry);

    emit(node);

}

string LLVMGetType (string type){
    if (type == "bool" || type == "BOOL"){
        return "i1";
    }
    else if (type == "int" || type == "INT"){
        return "i32";
    }
    else if (type == "byte" || type == "BYTE"){
        return "i8";
    }
    else if (type == "string" || type == "STRING"){
        return "i8*";
    }
    else if (type == "void" || type == "VOID"){
        return "void";
    }
    else{
        return "";
    }
}

void FuncDecl::emit(ASTNode *node) {
    string ret_type = LLVMGetType(this->ret_type_str);
    string parameter_list = "";
    if (this->arg_types.size() > 0){
        for(int i = 0; i < this->arg_types.size() - 1; i++) {
            parameter_list += LLVMGetType(this->arg_types.at(i));
            parameter_list += ", ";
        }
        parameter_list += LLVMGetType(this->arg_types.at(this->arg_types.size()-1));
    }
    CodeBuffer &buffer = CodeBuffer::instance();
    buffer.emit("define " + ret_type + " @" + node->value + "(" + parameter_list + ") {");
    RegisterManager &new_reg = RegisterManager::registerAlloc();
    string func_alloca_reg = new_reg.getNewRegister();
    Table &func_scope = tables_stack.back();
    func_scope.scope_reg = func_alloca_reg;
    buffer.emit(func_alloca_reg + " = alloca [50 x i32]");

}




/* OverRide Implementation */

//OverRide::OverRide(int line_no) : ASTNode("OverRide", line_no) {}

/* RetType Implementation */

///changed the node value here (it used to be "RetType") and it fixed an issue. maybe it's wrong
RetType::RetType(ASTNode *node) : ASTNode(node->value, node->line_no) {
 //   std::cout << "DEBUG RetType" << std::endl;
    g_return_type = node->value;

}

RetType::RetType(string type, int line_no) : ASTNode(type, line_no) {
  //  std::cout << "DEBUG RetType" << std::endl;
    g_return_type = type;
}



/* Formals Implementation */

Formals::Formals(int line_no) : ASTNode("Formals", line_no) {

}

Formals::Formals(FuncArgs* func_args) : ASTNode("Formals", func_args->line_no), func_args(func_args) {


//TODO: is this needed? this part exists in the ref



/// PROBLEM IS HERE: need to add tables_stack.push

////    FuncDecl func_decl() //TODO: talk to Timna
//    TableType func_type(true, func_decl);
//    TableEntry func_entry(g_function_name, func_type, 0);
//    tables_stack[0].table_entries_vec.push_back(func_entry);

//    for(Arg* current_arg : func_args->args_list)
//        std::cout << "current arg name: " << current_arg->arg_name << std::endl;

    int offset = -1;
    for (const auto &arg : func_args->args_list) {

        if (varIdTaken(arg->arg_name, nullptr)) {
            output::errorDef(func_args->line_no, arg->arg_name);
//            std::cout << "DEBUG here is arg instead of name" << std::endl;
            exit(0);
        }
        TableEntry arg_entry(arg->arg_name, TableType(false, arg->arg_type), offset);
        tables_stack.back().table_entries_vec.push_back(arg_entry);
        offset -= 1;
    }
}
/* Arg Implementation */

Arg::Arg(ASTNode* type, ASTNode* id) : ASTNode("Arg", id->line_no) {
    // is arg's id taken by curr func?
    if (g_function_name.compare(id->value) == 0) {
//        std::cout<<"erroDef 1"<<std::endl;
        output::errorDef(id->line_no, id->value);
        exit(0);
    }

    // is arg's id taken by any other func?
    for (const TableEntry& entry : tables_stack[0].table_entries_vec) {
        if (entry.name == id->value) {
//            std::cout<<"erroDef 2"<<std::endl;
            output::errorDef(id->line_no, id->value);
            exit(0);
        }
    }

    arg_type = type->value;
    arg_name = id->value;
}


/* FuncArgs Implementation */

FuncArgs::FuncArgs(int line_no) : ASTNode("FuncArgs", line_no) {}

FuncArgs::FuncArgs(Arg* arg) : ASTNode("FuncArgs", arg->line_no) {
    args_list = list<Arg*>{arg};
}

FuncArgs::FuncArgs(Arg* arg, FuncArgs* func_args) : ASTNode("FuncArgs", arg->line_no) {
    for (auto current_arg : func_args->args_list) {
//        std::cout << "main arg: " << arg->arg_name << std::endl;
//        std::cout << "curr iter: " << current_arg->arg_name << std::endl;
        if (current_arg->arg_name == arg->arg_name) {
            output::errorDef(func_args->line_no, arg->arg_name);
            exit(0);
        }
    }
    args_list = func_args->args_list;
    args_list.push_front(arg);
}

/* Expression Implementation */

string getDataTypeRepresentation(const string &dataType) {
    if (dataType == "void" || dataType == "VOID") {
        return "VOID";
    } else if (dataType == "int" || dataType == "INT") {
        return "INT";
    } else if (dataType == "byte" || dataType == "BYTE") {
        return "BYTE";
    } else if (dataType == "bool" || dataType == "BOOL") {
        return "BOOL";
    } else if (dataType == "string" || dataType == "STRING") {
        return "STRING";
    } else {
        // Handle the case when an unknown data type is encountered
        return ""; // or throw an exception, print an error message, etc.
    }
}

Expression::Expression(ASTNode* expression) : ASTNode("Call", expression->line_no) {
    bool found;
 //   std::cout << "DEBUG t17- problem with recursion" << std::endl;
    // std::cout<<" DEBUG mismatch 1"<<std::endl;
//    std::cout << "DEBUG this is the place of seg 4" << std::endl;
//    std::cout << "DEBUG expression value: "<<expression->value << std::endl;
    bool ambiguous = false;
    TableEntry func_entry = getFunctionEntry(expression, nullptr, nullptr, &found, &ambiguous);
    if (!found) {
        output::errorUndefFunc(expression->line_no, expression->value);
        exit(0);
    }
    if (ambiguous) {
        output::errorAmbiguousCall(expression->line_no, expression->value);
        exit(0);
    }
    this->type_name = func_entry.type.func_decl->ret_type_str;
    g_exp_type = this->type_name;
    vector<string> params = func_entry.type.func_decl->arg_types;
    vector<string> arg_caps;
    if (!params.empty()) {
        for (string type : params) {
            arg_caps.push_back(getDataTypeRepresentation(type));
        }
        if (params.size() != 0){
           // std::cout<<"mismatch 1"<<std::endl;
            output::errorPrototypeMismatch(expression->line_no, expression->value);
            exit(0);
        }

    }
}

Expression::Expression(ASTNode* node, string type) : ASTNode(node->value, node->line_no), type_name(type) {
//    std::cout << "DEBUG ENTERED EXP C'TOR" << std::endl;
    this->type_name = type;
    g_exp_type = this->type_name;

    if (type_name == "id") {
        TableType id_type;
//        std::cout << "DEBUG this is the place of seg 1" << std::endl;
        if (!varIdTaken(node->value, &id_type)) {
            output::errorUndef(node->line_no, node->value);
            exit(0);
        }
        this->type_name = id_type.variable_type;
        g_exp_type = this->type_name;
    }
    else if (type_name == "byte") {
        int value = stoi(node->value);
        if (value > BYTE_MAX_SIZE)
        {
            output::errorByteTooLarge(node->line_no, node->value);
            exit(0);
        }
    }
}


Expression::Expression(ASTNode* expression, ExpList* explist) : ASTNode("Call", expression->line_no) {
    bool found = false;
    bool ambiguous = false;
    TableEntry func_entry = getFunctionEntry(expression, explist, nullptr, &found, &ambiguous);
//    std::cout<<" DEBUG test 2: "<< explist->exp_list.back()->type_name << std::endl;
    if (ambiguous){
        output::errorAmbiguousCall(expression->line_no, expression->value);
        exit(0);
    }
    if (!found) {
//        std::cout<<" DEBUG 3"<<std::endl;
//        std::cout<<" line no: " << expression->line_no<<std::endl;

        output::errorUndefFunc(expression->line_no, expression->value);
        exit(0);
    }

    this->type_name = func_entry.type.func_decl->ret_type_str;

    g_exp_type = this->type_name;
    vector<string> params = func_entry.type.func_decl->arg_types;

//    for (string str : params)
//        std::cout << "DEBUG token is:" << getDataTypeRepresentation(str) << std::endl;

    if (!params.empty()) {
        //std::cout << "params.size(): " << params.size() << std::endl;
        //std::cout << "explist->exp_list.size() " << explist->exp_list.size() << std::endl;
        if (params.size() != explist->exp_list.size()) {
//            std::cout<<"size of params: "<< params.size() <<std::endl;
        //    std::cout << "mismatch 2" << std::endl;
            output::errorPrototypeMismatch(expression->line_no, expression->value);
            exit(0);
        }
    }
    ///PROBLEM HERE!!! TYPE CAN ALSO BE A FUNC'S RETURN TYPE


    for (int i = 0; i < params.size(); i++) {
        Expression* arg = explist->exp_list[i];
//        std::cout<<"arg->type_name: " << arg->type_name <<std::endl;
//        std::cout<<"params[i]: " << params[i] <<std::endl;
        if (arg->type_name != params[i] && !(arg->type_name == "byte" && params[i] == "int")) {
        //    std::cout << "mismatch 3" << std::endl;
            output::errorPrototypeMismatch(expression->line_no, expression->value);
            exit(0);
        }
    }
}



Expression::Expression(ASTNode* node, string operation, Expression* expression) : ASTNode(expression->value, node->line_no) {
    if (operation == "not"){
        if (!(expression->type_name == "bool")){
            // std::cout<<"mismatch 4"<<std::endl;
            output::errorMismatch(node->line_no);
            exit(0);
        }
        this->type_name = "bool";
        g_exp_type = this->type_name;
    }
    if (operation == "cast" && (expression->type_name == "int" || expression->type_name == "byte")){
        if (node->value == "byte"){
            this->type_name = "byte";
            g_exp_type = this->type_name;
        }
        else if (node->value == "int"){
            this->type_name = "int";
            g_exp_type = this->type_name;
        }
        else {
//            std::cout<<"mismatch 5"<<std::endl;
            output::errorMismatch(node->line_no);
            exit(0);
        }
    }
}

Expression::Expression(ASTNode *node, string type_name, string operation, Expression* exp1, Expression* exp2) : ASTNode(type_name, node->line_no) {
    if (operation == "and" || operation == "or"){
        if (exp1->type_name != "bool" || exp2->type_name != "bool"){
//            std::cout<<"mismatch 6"<<std::endl;
            output::errorMismatch(node->line_no);
            exit(0);
        }
        this->type_name = "bool";
        g_exp_type = this->type_name;
    }
    else if (operation == "binop"){
//        std::cout << "DEBUG entering binop clause" << std::endl;
        this->type_name = "int"; //default
        if (exp1->type_name == "byte" && exp2->type_name == "byte"){
            this->type_name = "byte";
            g_exp_type = this->type_name;
        }
        else if (!((exp1->type_name == "int" && exp2->type_name == "int") || (exp1->type_name == "int" && exp2->type_name == "byte") || (exp1->type_name == "byte" && exp2->type_name == "int")))
        {
//            std::cout<<"mismatch 7"<<std::endl;
            output::errorMismatch(node->line_no);
            exit(0);
        }
//        std::cout << "DEBUG exited binop clause" << std::endl;
    }
    else if (operation == "relop"){
        if (!((exp1->type_name == "int" || exp1->type_name == "byte") && (exp2->type_name == "int" || exp2->type_name == "byte")))
        {
//            std::cout<<"mismatch 8"<<std::endl;
            output::errorMismatch(node->line_no);
            exit(0);
        }
        this->type_name = "bool";
        g_exp_type = this->type_name;
    }
}

///TODO: if not harming- delete!
//Expression::Expression(string operation, Expression* exp1, Expression* exp2, Expression* exp3) : ASTNode("Trinary", exp1->line_no) {
//    if (exp2->type_name != "bool")
//    {
//        std::cout<<"mismatch 9"<<std::endl;
//        output::errorMismatch(exp2->line_no);
//        exit(0);
//    }
//    string new_type = exp1->type_name;
//    if (exp1->type_name == exp3->type_name)
//    {
//        this->type_name = new_type;
//        g_exp_type = this->type_name;
//    }
//    else if ((exp1->type_name == "int") && (exp3->type_name == "byte") || (exp1->type_name == "byte") && (exp3->type_name == "int"))
//    {
//        this->type_name = "int";
//        g_exp_type = this->type_name;
//    }
//    else
//    {
//        std::cout<<"mismatch 10"<<std::endl;
//        output::errorMismatch(exp2->line_no);
//        exit(0);
//    }
//}

/* ExpList Implementation */

ExpList::ExpList(Expression* expression) : ASTNode(expression->value, expression->line_no) {
    exp_list.push_back(expression);
}

ExpList::ExpList(Expression* expression, ExpList* list) : ASTNode(expression->value, expression->line_no) {
    this->exp_list.push_back(expression);
    this->exp_list.insert(this->exp_list.end(), list->exp_list.begin(), list->exp_list.end());
}

/* Statement Implementation */

Statement::Statement(ASTNode* statement) : ASTNode("Statement", statement->line_no) {
    CodeBuffer &buffer = CodeBuffer::instance();
    this->nextlist = statement->nextlist;
    this->breaklist = statement->breaklist;
    this->continuelist = statement->continuelist;
    int end_of_statement = buffer.emit("br label @");
    this->nextlist.push_back(make_pair(end_of_statement, FIRST));
}

/* Statements Implementation */
Statements::Statements(Statement *statement) : ASTNode("Statements", statement->line_no){
    this->nextlist = statement->nextlist;
    this->breaklist = statement->breaklist;
    this->continuelist = statement->continuelist;
}

Statements::Statements(Statements *statements, LabelM *label_m, Statement *statement) : ASTNode("Statements", statement->line_no){
    CodeBuffer &buffer = CodeBuffer::instance();
    buffer.bpatch(statements->nextlist, label_m)
}

/* OpenStatement Implementation */

OpenStatement::OpenStatement(Expression* statement) : ASTNode("OpenStatement", statement->line_no) {}

/* ClosedStatement Implementation */

ClosedStatement::ClosedStatement(Expression* statement) : ASTNode("ClosedStatement", statement->line_no) {}
ClosedStatement::ClosedStatement(int line_no) : ASTNode("ClosedStatement", line_no) {}
/* SomeStatement Implementation */

SomeStatement::SomeStatement(ASTNode *type, ASTNode *id) : ASTNode("SomeStatement", id->line_no) {
    if (varIdTaken(id->value)) {
//        std::cout<<"erroDef 3"<<std::endl;
        output::errorDef(id->line_no, id->value);
        exit(0);
    }

    bool found = false;
    bool ambiguous = false;
    TableEntry func_entry = getFunctionEntry(id, nullptr, nullptr, &found, &ambiguous);
    if (found) {
        output::errorDef(id->line_no, id->value);
        exit(0);
    }

    addVarToStack(id->value, type->value);
}


SomeStatement::SomeStatement(ASTNode *type, ASTNode *id, Expression *expression) : ASTNode("SomeStatement", id->line_no) // 16
{
    string lh_type = type->value;
    string rh_type = expression->type_name;

    if (varIdTaken(id->value)) {
        output::errorDef(id->line_no, id->value);
        exit(0);
    }

    bool found = false;
    bool ambiguous = false;
    TableEntry func_entry = getFunctionEntry(id, nullptr, nullptr, &found, &ambiguous);
    if (found) {
        output::errorDef(id->line_no, id->value);
        exit(0);
    }

     else if (lh_type != rh_type && !(lh_type == "int" && rh_type == "byte")) {
//        std::cout<<"mismatch 11"<<std::endl;
        output::errorMismatch(id->line_no);
        exit(0);
    }
    addVarToStack(id->value, type->value);
}

SomeStatement::SomeStatement(string str, Expression *expression) : ASTNode("SomeStatement", expression->line_no)
{
//    std::cout << "DEBUG SomeStatement CTOR!1" << std::endl;
    if (str == "return") {
        if (g_return_type == "void" || (!(g_return_type == "int" && expression->type_name == "byte") && g_return_type != expression->type_name)) {
//            std::cout<<"glob ret = "<< g_return_type << std::endl;
//            std::cout<<"expression->type_name = "<< expression->type_name << std::endl;
            output::errorMismatch(expression->line_no);
            exit(0);
        }
    }
    else {
        TableType type;
//        std::cout << "DEBUG this is the place of seg 3" << std::endl;

        if (!varIdTaken(str, &type)) {
            output::errorUndef(expression->line_no, str);
            exit(0);
        }

        string left_type = type.variable_type;
        string right_type = expression->type_name;
        if (left_type != right_type && !(left_type == "int" && right_type == "byte")) {
//            std::cout<<"mismatch 13"<<std::endl;
            output::errorMismatch(expression->line_no);
            exit(0);
        }
    }
}

SomeStatement::SomeStatement(ASTNode *node) : ASTNode("SomeStatement", node->line_no)
{
    if (node->value == "call" || node->value == "{") {
        // Do nothing for "call" or "{"
    }
    else if (node->value == "return") {
        if (g_return_type != "VOID") /*Should be VOID? All caps? - YES! */ {
//            std::cout<<"mismatch 14"<<std::endl;
            output::errorMismatch(node->line_no);
            exit(0);
        }
    }
    else if (node->value == "break") {
        if (!tables_stack.back().contains_while_loop) {
            output::errorUnexpectedBreak(node->line_no);
            exit(0);
        }
    }
    else if (node->value == "continue") {
        if (!tables_stack.back().contains_while_loop) {
            output::errorUnexpectedContinue(node->line_no);
            exit(0);
        }
    }
}

/*Markers Implementation*/
LabelM::LabelM() : ASTNode("Marker", 0) {
    CodeBuffer &buffer = CodeBuffer::instance();
    int print_line = buffer.emit("br label @");
    this->label = buffer.genLabel();
    buffer.bpatch(buffer.makelist(make_pair(print_line, FIRST)), this->label);
}

ExitM::ExitM() : ASTNode("Marker", 0){
    CodeBuffer &buffer = CodeBuffer::instance();
    int print_line = buffer.emit("br label @");
    this->nextlist = buffer.makelist(make_pair(print_line, FIRST));
}


/* Table Implementation */

/* TableEntry Implementation */
//
//TableEntry::TableEntry() : offset(0) {}
//
//TableEntry::TableEntry(string name, TableType type, int offset) : name(name), type(type), offset(offset) {}
//
//TableEntry::TableEntry(const TableEntry& table_entry) : name(table_entry.name), type(table_entry.type), offset(table_entry.offset) {}

/* Auxiliaries Implementation */

void validateMain(){
  //  std::cout<< "DEBUG validateMain" <<std::endl;
    Table top_table = tables_stack[0];
    int size = top_table.table_entries_vec.size();
    TableEntry current_entry;
    for (int i = 0; i < size; i++){
        current_entry = top_table.table_entries_vec[i];
//        std::cout<< "DEBUG num args: " << current_entry.type.func_decl->num_args << std::endl;
//        std::cout<< "DEBUG function_type: " << current_entry.type.function_type << std::endl;
//        std::cout<< "DEBUG name: " << current_entry.name << std::endl;
        if (current_entry.type.function_type){
            if (current_entry.name == "main"){
                if (current_entry.type.func_decl->arg_types.size() != 0 || current_entry.type.func_decl->ret_type_str != "VOID"){
                    output::errorMainMissing();
                    exit(0);
                }
                return;
            }
        }
    }
    output::errorMainMissing();
    exit(0);
}

void initLLVM(){
    CodeBuffer &buffer = CodeBuffer::instance();
    buffer.emit("");
    buffer.emit("declare i32 @printf(i8*, ...)");
    buffer.emit("declare void @exit(i32)");
    buffer.emit("@.int_specifier = constant [4 x i8] c\"%d\\0A\\00\"");
    buffer.emit("@.str_specifier = constant [4 x i8] c\"%s\\0A\\00\"");
    buffer.emit("define void @printi(i32) {");
    buffer.emit("%spec_ptr = getelementptr [4 x i8], [4 x i8]* @.int_specifier, i32 0, i32 0");
    buffer.emit("call i32 (i8*, ...) @printf(i8* %spec_ptr, i32 %0)");
    buffer.emit("ret void");
    buffer.emit("}");
    buffer.emit("define void @print(i8*) {");
    buffer.emit("%spec_ptr = getelementptr [4 x i8], [4 x i8]* @.str_specifier, i32 0, i32 0");
    buffer.emit("call i32 (i8*, ...) @printf(i8* %spec_ptr, i8* %0)");
    buffer.emit("ret void");
    buffer.emit("}");
   // buffer.emit("");
   // buffer.emit("");
}

void initGlobalScope()
{
 //   std::cout<< "DEBUG initGlobalScope" <<std::endl;
    vector<string> p_args = {"string"};
    vector<string> pi_args = {"int"};
    FuncDecl* p_decl = new FuncDecl(0, "VOID", false, 1, &p_args);
    FuncDecl* pi_decl = new FuncDecl(0, "VOID", false, 1, &pi_args);
    TableType ttype_p(true, p_decl);
    TableType ttype_pi(true, pi_decl);
    TableEntry p_entry("print", ttype_p, 0);
    TableEntry pi_entry("printi", ttype_pi, 0);
    Table t;
    t.table_entries_vec.push_back(p_entry);
    t.table_entries_vec.push_back(pi_entry);
    tables_stack.push_back(t);
    offsets_stack.push_back(0);
}

void newScope()
{
  //  std::cout<< "DEBUG newScope" <<std::endl;
    offsets_stack.push_back(offsets_stack.back()); //try2
    Table t;
    t.contains_while_loop = tables_stack.back().contains_while_loop;
    tables_stack.push_back(t);
    t.scope_reg = tables_stack.back().scope_reg;
}

void newWhileScope()
{
    offsets_stack.push_back(offsets_stack.back()); //try1
    Table t;
    t.contains_while_loop = true;
    tables_stack.push_back(t);
    t.scope_reg = tables_stack.back().scope_reg;
}


void isBool(int line_number)
{
    //cout << global_exp_type << endl;
    if (g_exp_type.compare("bool") != 0)
    {
//        std::cout<<"mismatch 15"<<std::endl;
        output::errorMismatch(line_number);
        exit(0);
    }
}


bool varIdTaken(string id_name, TableType *id_type){
    Table *current_table;
//    std::cout << "DEBUG looking for id_name: " << id_name << std::endl;
    // excluding global

    for (int i = tables_stack.size()-1; i > 0; i--) {
        current_table = &tables_stack[i];
//        std::cout << "i: " << i << std::endl;
//        std::cout << "DEBUG tables_stack.size: " << tables_stack.size() << std::endl;

        for (int j = 0; j < current_table->table_entries_vec.size(); j++) {
//            std::cout << "vector's size is: " << current_table->table_entries_vec.size() << std::endl;
//            std::cout << "j: " << j << std::endl;
            TableEntry *entry = &current_table->table_entries_vec[j];
//            std::cout << "HI!!! entry->name: " << entry->name << std::endl;
            if (entry->name == id_name) {
                if (id_type != nullptr) {
                    *id_type = entry->type;
//                    std::cout << "DEBUG all good with " << id_name << std::endl;
                }


                return true;
            }
        }

    }
    return false;
}




void printScopesSymbols(){
  //  std::cout<< "DEBUG printScopesSymbols" <<std::endl;
    output::endScope();
    Table *curr_scope = &tables_stack.back();
    for (int i=0; i<curr_scope->table_entries_vec.size(); i++){
        TableEntry *curr_entry = &curr_scope->table_entries_vec[i];
        if (curr_entry->type.function_type){
            vector<string> args;
            if (curr_entry->type.func_decl->arg_types.size() != 0){
                for (string type : curr_entry->type.func_decl->arg_types) {
                    // PROBLEM! DOESN'T GET HERE
                    args.push_back(getDataTypeRepresentation(type));
                }
            }
            output::printID(curr_entry->name, curr_entry->offset, output::makeFunctionType(getDataTypeRepresentation(curr_entry->type.func_decl->ret_type_str), args));
        }
        else {
            output::printID(curr_entry->name, curr_entry->offset, getDataTypeRepresentation(curr_entry->type.variable_type));
        }
    }
 //   std::cout<< "DEBUG printScopesSymbols is ended" <<std::endl;
}

void addVarToStack(string name, string type)
{
    // Create a new variable entry
    int new_offset = offsets_stack.back();
    TableType new_type(false, type);
    TableEntry new_entry(name, new_type, new_offset);

    // Add the variable entry to the current table in the table stack
    (tables_stack.back()).table_entries_vec.push_back(new_entry);

    // Update the offset stack
    offsets_stack.back() += 1;
}

// added explist, probably not the best.. need to debug
TableEntry getFunctionEntry(ASTNode *node,ExpList* explist, FuncArgs* funcargs, bool* found, bool *ambiguous) {
    Table g_table = tables_stack[0];
    TableEntry current_entry;
    TableEntry found_entry;
    for (int i = 0; i <  g_table.table_entries_vec.size(); i++) {
        current_entry = g_table.table_entries_vec[i];
        if (current_entry.type.function_type && current_entry.name == node->value) {
            if(current_entry.type.func_decl->isOverride) {
                if(explist) {
                    if (current_entry.type.func_decl->arg_types.size() == explist->exp_list.size()) {

                        bool match = true;
//                        std::cout<<"func is: "<<current_entry.name<<std::endl;
                        for (int i=0; i< explist->exp_list.size() ; i++){
                            if (current_entry.type.func_decl->arg_types.at(i) != explist->exp_list.at(i)->type_name){
                                if (!((current_entry.type.func_decl->arg_types.at(i) == "int" && explist->exp_list.at(i)->type_name == "byte"))){
                                    match = false;


                                }
                            }
                        }
//                        std::cout<<"here"<<std::endl;
                        if (match){
                            if (*found){
                                *ambiguous = true;
                                return found_entry;
                            }
                            *found = true;
                            found_entry = current_entry;
                        }

                    }
                }
                else if (funcargs) {
                    if (current_entry.type.func_decl->arg_types.size() == funcargs->args_list.size()) {
                        if (*found){

                            *ambiguous = true;
                            return found_entry;
                        }
                        *found = true;
                        found_entry = current_entry;
                    }
                }
                else { //both null

                    if (current_entry.type.func_decl->arg_types.size() == 0){
                        *found = true;
                        return current_entry;
                    }

                }
            }
            else {
                *found = true;
                return current_entry;
            }
        }
    }
    if (*found){
        return found_entry;
    }
    *found = false;
    return current_entry;
}