#include "class_defs.hpp"
#include <iostream>
//#include "bp.hpp"
using namespace std;

string g_return_type;
string g_function_name;
string g_exp_type;
bool bool_size;
vector<Table> tables_stack;
vector<int> offsets_stack;
const int BYTE_MAX_SIZE = 255;


/* ASTNode Implementation */

ASTNode::ASTNode(string value, int line_no) : value(value), line_no(line_no) {}

/* FuncDecl Implementation */
FuncDecl::FuncDecl(int line_no, string ret_type_str, bool is_override, int num_args, vector<string> *arg_types) :
        ASTNode("FuncDecl", line_no), ret_type(nullptr), ret_type_str(ret_type_str), isOverride(is_override), num_args(num_args), arg_types(*arg_types) {}

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

    func_name_exists = false;
    bool ambiguous = false;
    TableEntry override_func = getFunctionEntry(node, nullptr, formals->func_args, &func_name_exists, &ambiguous);
    if (func_name_exists) {
        if (isOverride && !override_func.type.func_decl->isOverride) {
            output::errorFuncNoOverride(node->line_no, g_function_name);
            exit(0);
        } else if ((isOverride && override_func.type.func_decl->isOverride
                   && arg_types.size() == override_func.type.func_decl->arg_types.size()
                   && arg_types == override_func.type.func_decl->arg_types
                   && ret_type_str == override_func.type.func_decl->ret_type_str)
                   || ( !isOverride && !override_func.type.func_decl->isOverride)) { // both aren't override
            output::errorDef(node->line_no, g_function_name);
            exit(0);
        }
    }
    TableType func_type(true, this);
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
    else {
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

void FuncDecl::endFunc(Statements *statements){
    CodeBuffer &buffer = CodeBuffer::instance();
    string ret_type = LLVMGetType(this->ret_type_str);
    buffer.emit(";end func in func decl label");

    string end_label = buffer.genLabel();
    buffer.bpatch((statements->nextlist), end_label);
    if (ret_type != "void") {
        buffer.emit("ret " + ret_type + " 0");
    } else {
        buffer.emit("ret " + ret_type);
    }
    buffer.emit("}");
}

/* RetType Implementation */

RetType::RetType(ASTNode *node) : ASTNode(node->value, node->line_no) {
    g_return_type = node->value;

}

RetType::RetType(string type, int line_no) : ASTNode(type, line_no) {
    g_return_type = type;
}


/* Formals Implementation */

Formals::Formals(int line_no) : ASTNode("Formals", line_no) {}

Formals::Formals(FuncArgs* func_args) : ASTNode("Formals", func_args->line_no), func_args(func_args) {
    int offset = -1;
    for (const auto &arg : func_args->args_list) {

        if (varIdTaken(arg->arg_name, nullptr)) {
            output::errorDef(func_args->line_no, arg->arg_name);
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
        output::errorDef(id->line_no, id->value);
        exit(0);
    }

    // is arg's id taken by any other func?
    for (const TableEntry& entry : tables_stack[0].table_entries_vec) {
        if (entry.name == id->value) {
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
        return "";
    }
}

Expression::Expression(ASTNode* expression) : ASTNode("Call", expression->line_no) {
    bool found;
    bool ambiguous = false;
    TableEntry func_entry = getFunctionEntry(expression, nullptr, nullptr, &found, &ambiguous);
    CodeBuffer &buffer = CodeBuffer::instance();
    RegisterManager &reg_m = RegisterManager::registerAlloc();
    string reg = reg_m.getNewRegister();
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
            output::errorPrototypeMismatch(expression->line_no, expression->value);
            exit(0);
        }
    }
//    start_line = buffer.emit("br label @");
//    start_label = buffer.genLabel();

    if(g_exp_type == "void" || g_exp_type == "VOID") {
        buffer.emit("call void @" + func_entry.name + "()");
    } else {
        buffer.emit(reg + " call " + LLVMGetType(g_exp_type) + " @" + func_entry.name + "()");
        store_loc = reg;
    }
    if (g_exp_type == "bool" || g_exp_type == "BOOL") {
        int cond_line = buffer.emit("br i1 " + reg + ", label @, label @");
        truelist = buffer.makelist(make_pair(cond_line, FIRST));
        falselist = buffer.makelist(make_pair(cond_line, SECOND));
    }

//    end_line = buffer.emit("br label @");
//    end_label = buffer.genLabel();
}

Expression::Expression(ASTNode* node, string type) : ASTNode(node->value, node->line_no), type_name(type) {
    this->type_name = type;
    g_exp_type = this->type_name;
    CodeBuffer &buffer = CodeBuffer::instance();
    RegisterManager &reg_m = RegisterManager::registerAlloc();
    if (type_name == "id") {
        TableType id_type;
        if (!varIdTaken(node->value, &id_type)) {
            output::errorUndef(node->line_no, node->value);
            exit(0);
        }

        buffer.emit(";DEBUG! Expression::Expression(" + node->value + ")");

//        start_line = buffer.emit("br label @");
//        start_label = buffer.genLabel();

        this->type_name = id_type.variable_type;
        g_exp_type = this->type_name;
        // look in current scope if id is a function parameter

        bool found = false;
        TableEntry id_entry;

        buffer.emit(";DEBUG id is: " + node->value);
//        buffer.emit("; scope size is: " + to_string(current_table->table_entries_vec.size()));
        Table *current_table;
        for (int i = tables_stack.size()-1; i > 0; i--) {
            current_table = &tables_stack[i];
            for (int j = 0; j < current_table->table_entries_vec.size(); j++) {
                TableEntry *entry = &current_table->table_entries_vec[j];
                if (entry->name == node->value) {
                    found =  true;
                    id_entry = *entry;
                }
            }
        }


        if (found) { // id is a function parameter
            buffer.emit(";DEBUG id is a function parameter");
            if (id_entry.offset < 0){
                int param_index = (-1*id_entry.offset) -1;
                this->store_loc = "%" + to_string(param_index);

                if (type_name == "bool") {
                    int tmp_br = buffer.emit("br i1 " + store_loc + ", label @, label @");
                    truelist.push_back(make_pair(tmp_br, FIRST));
                    falselist.push_back(make_pair(tmp_br, SECOND));
                }
            }
        }
        else { //need to load from memory
            this->store_loc = reg_m.getNewRegister();
            string address = reg_m.getNewRegister();
            buffer.emit(address + " = getelementptr [50 x i32], [50 x i32]* " + tables_stack.back().scope_reg + ", i32 0, i32 " +
                                to_string(id_entry.offset) + ";DEBUG3");
            buffer.emit(this->store_loc + " = load i32, i32* " + address);

            if (type_name == "bool") {
                string clause = reg_m.getNewRegister();
                buffer.emit(clause + " = trunc i32 " + store_loc + " to i1");
                int tmp_br = buffer.emit("br i1 " + clause + ", label @, label @");

                truelist.push_back(make_pair(tmp_br, FIRST));
                falselist.push_back(make_pair(tmp_br, SECOND));
            }

            else if (type_name == "byte") {
                string trunc_reg = reg_m.getNewRegister();
                buffer.emit(trunc_reg + " = trunc i32 " + store_loc + " to i8");
                store_loc = trunc_reg;
            }
        }

//        end_line = buffer.emit("br label @");
//        end_label = buffer.genLabel();
    }
    else if (type_name == "byte") {
        int value = stoi(node->value);
        if (value > BYTE_MAX_SIZE) {
            output::errorByteTooLarge(node->line_no, node->value);
            exit(0);
        }
        this->store_loc = node->value;
//        start_line = buffer.emit("br label @ ;start_line");
//        start_label = buffer.genLabel();
//        end_line = buffer.emit("br label @ ;end_line");
//        end_label = buffer.genLabel();
    }
    else if (type_name == "int") {
        this->store_loc = node->value;
//        start_line = buffer.emit("br label @ ;start_line");
//        start_label = buffer.genLabel();
//        end_line = buffer.emit("br label @ ;end_line");
//        end_label = buffer.genLabel();
    }
    else if (type_name == "bool") {

//        start_line = buffer.emit("br label @ ;start_line");
//        start_label = buffer.genLabel();
        if (this->value == "true") {
            int br = buffer.emit("br label @ ; true");
            this->truelist = buffer.makelist(make_pair(br, FIRST));
        }
        else if (this->value == "false") {
            int br = buffer.emit("br label @ ; false");
            this->falselist = buffer.makelist(make_pair(br, SECOND));
        }
//        end_line = buffer.emit("br label @ ;end_line");
//        end_label = buffer.genLabel();
    }
    else if (type_name == "string") {
//        start_line = buffer.emit("br label @ ;start_line");
//        start_label = buffer.genLabel();
        string str = reg_m.getNewString();
        int str_len = node->value.size() - 1;
        string str_len_str = to_string(str_len);

        buffer.emitGlobal(str + " = constant [" + str_len_str + " x i8] c" + node->value.substr(0,str_len) + "\\00\"");
        store_loc = reg_m.getNewRegister();
        buffer.emit(this->store_loc + " = getelementptr [" + str_len_str + " x i8], [" + str_len_str + " x i8]* " + str + ", i32 0, i32 0");
//        end_line = buffer.emit("br label @ ;end_line");
//        end_label = buffer.genLabel();
    }
}


Expression::Expression(ASTNode* expression, ExpList* explist) : ASTNode("Call", expression->line_no) {
    bool found = false;
    bool ambiguous = false;
    TableEntry func_entry = getFunctionEntry(expression, explist, nullptr, &found, &ambiguous);
    if (ambiguous){
        output::errorAmbiguousCall(expression->line_no, expression->value);
        exit(0);
    }
    if (!found) {
        output::errorUndefFunc(expression->line_no, expression->value);
        exit(0);
    }

    this->type_name = func_entry.type.func_decl->ret_type_str;

    g_exp_type = this->type_name;
    vector<string> params = func_entry.type.func_decl->arg_types;

    if (!params.empty()) {
        if (params.size() != explist->exp_list.size()) {
            output::errorPrototypeMismatch(expression->line_no, expression->value);
            exit(0);
        }
    }
    for (int i = 0; i < params.size(); i++) {
        Expression* arg = explist->exp_list[i];
        if (arg->type_name != params[i] && !(arg->type_name == "byte" && params[i] == "int")) {
            output::errorPrototypeMismatch(expression->line_no, expression->value);
            exit(0);
        }
    }

    CodeBuffer &buffer = CodeBuffer::instance();
    RegisterManager &reg_m = RegisterManager::registerAlloc();
//    start_line = buffer.emit("br label @ ;start_line");
//    start_label = buffer.genLabel();
    string reg = reg_m.getNewRegister();
    string args_str = " ";
    string expression_reg;
    for (int i = 0; i < params.size(); i++){
        args_str += LLVMGetType(params[i]);
        args_str += " ";
        string rh_data_type = getDataTypeRepresentation(params[i]);
        if (rh_data_type == "BOOL") {
            expression_reg = getBoolReg(explist->exp_list[i], false);
        } else if (rh_data_type == "BYTE") {
            buffer.emit(expression_reg + " = zext i8 " + explist->exp_list[i]->store_loc + " to i32"); //zero extension
        } else if (rh_data_type == "INT") {
            buffer.emit(expression_reg + " = add i32 " + explist->exp_list[i]->store_loc + ", 0;1"); //add zero
        } else if (rh_data_type == "STRING") {
            expression_reg = explist->exp_list[i]->store_loc;
        }
        args_str += expression_reg;
        args_str += ", ";
    }
    args_str = args_str.substr(0, args_str.size() - 2);

    if (func_entry.type.func_decl->ret_type_str == "VOID") {
        buffer.emit("call " + LLVMGetType(this->type_name) + " @" + func_entry.name + "(" + args_str + ")");
    }
    else {
        buffer.emit(reg + " = call " + LLVMGetType(this->type_name) + " @" + func_entry.name + "(" + args_str + ")");
    }
    if (func_entry.type.func_decl->ret_type_str == "bool"){
        string condition = reg_m.getNewRegister();
        int br = buffer.emit("br i1 " + reg + ", label @, label @");
        this->truelist = buffer.makelist(make_pair(br, FIRST));
        this->falselist = buffer.makelist(make_pair(br, SECOND));
    }
    else {
        this->store_loc = reg;
    }
//    end_line = buffer.emit("br label @ ;end_line");
//    end_label = buffer.genLabel();
}

Expression::Expression(ASTNode* node, string operation, Expression* expression) : ASTNode(expression->value, node->line_no) {
    CodeBuffer &buffer = CodeBuffer::instance();
    RegisterManager &reg_m = RegisterManager::registerAlloc();
    if (operation == "not"){
        if (!(expression->type_name == "bool")){
            output::errorMismatch(node->line_no);
            exit(0);
        }
        this->type_name = "bool";
        g_exp_type = this->type_name;
        this->truelist = expression->falselist;
        this->falselist = expression->truelist;
    }
    string reg = expression->store_loc;
    if (operation == "cast" && (expression->type_name == "int" || expression->type_name == "byte")){
        if (node->value == "byte"){
            this->type_name = "byte";
            g_exp_type = this->type_name;
            if (expression->type_name == "int"){
                reg = reg_m.getNewRegister();
                buffer.emit(reg + " = trunc i32 " + expression->store_loc + " to i8");
            }
            this->store_loc = reg;
        }
        else if (node->value == "int"){
            this->type_name = "int";
            g_exp_type = this->type_name;
            if (expression->type_name == "byte"){
                reg = reg_m.getNewRegister();
                buffer.emit(reg + " = zext i8 " + expression->store_loc + " to i32");
            }
            this->store_loc = reg;
        }
        else {
            output::errorMismatch(node->line_no);
            exit(0);
        }
    }
//    start_line = buffer.emit("br label @ ;start_line");
//    start_label = buffer.genLabel();
//    buffer.bpatch(buffer.makelist(make_pair(expression->start_line, FIRST)), expression->start_label);
//    buffer.bpatch(buffer.makelist(make_pair(expression->end_line, FIRST)), expression->end_label);
//    end_line = buffer.emit("br label @ ;end_line");
//    end_label = buffer.genLabel();
}

Expression::Expression(ASTNode *node, string type_name, string operation, Expression* exp1, Expression* exp2) : ASTNode(type_name, node->line_no) {
    CodeBuffer &buffer = CodeBuffer::instance();
    RegisterManager &reg_m = RegisterManager::registerAlloc();
    string exp1_loc = exp1->store_loc;
    string exp2_loc = exp2->store_loc;
    string typeLLVM = "i32"; // default
    string op;
    //commented this out since it's handeled in another c'tor
//    if (operation == "and" || operation == "or"){
//        if (exp1->type_name != "bool" || exp2->type_name != "bool"){
//            output::errorMismatch(node->line_no);
//            exit(0);
//        }
//        this->type_name = "bool";
//        g_exp_type = this->type_name;
//    }
    if (operation == "binop"){
        this->type_name = "int"; //default
        if (exp1->type_name == "byte" && exp2->type_name == "byte"){
            this->type_name = "byte";
            g_exp_type = this->type_name;
        }
        else if (!((exp1->type_name == "int" && exp2->type_name == "int") ||
                (exp1->type_name == "int" && exp2->type_name == "byte") ||
                (exp1->type_name == "byte" && exp2->type_name == "int"))) {
            output::errorMismatch(node->line_no);
            exit(0);
        }


//        buffer.bpatch(buffer.makelist(make_pair(exp1->start_line, FIRST)), exp1->start_label);
//        buffer.bpatch(buffer.makelist(make_pair(exp1->end_line, FIRST)), exp1->end_label);
//        buffer.bpatch(buffer.makelist(make_pair(exp2->start_line, FIRST)), exp2->start_label);
//        buffer.bpatch(buffer.makelist(make_pair(exp2->end_line, FIRST)), exp2->end_label);
//        start_line = buffer.emit("br label @ ;start_line");
//        start_label = buffer.genLabel();

        if (exp1->type_name == "byte" && exp2->type_name == "int"){
            string ext_exp1 = reg_m.getNewRegister();
            buffer.emit(ext_exp1 + " = zext i8 " + exp1_loc + " to i32");
            exp1_loc = ext_exp1;
        }
        else if (exp2->type_name == "byte" && exp1->type_name == "int"){
            string ext_exp2 = reg_m.getNewRegister();
            buffer.emit(ext_exp2 + " = zext i8 " + exp2_loc + " to i32");
            exp2_loc = ext_exp2;
        } else{
            typeLLVM = LLVMGetType(exp1->type_name);
        }

        if (node->value == "+"){
            op = "add";
        }
        else if (node->value == "-"){
            op = "sub";
        }
        else if (node->value == "*"){
            op = "mul";
        }
        else if (node->value == "/"){
            //check if dividing by zero
            string reg = reg_m.getNewRegister();
            buffer.emit(reg + " = icmp eq " + typeLLVM + " 0, " + exp2_loc);
            int bp = buffer.emit("br i1 " + reg + ", label @, label @");

            string true_label = buffer.genLabel();
            buffer.emit("call void (i8*) @print(i8* getelementptr ([23 x i8], [23 x i8]* @.div_by_zero_err_msg, i32 0, i32 0))");
            buffer.emit("call void (i32) @exit(i32 0)");
            int br = buffer.emit("br label @ ;8");
            string false_label = buffer.genLabel();
            buffer.bpatch(buffer.makelist(make_pair(bp, FIRST)), true_label);
            buffer.bpatch(buffer.makelist(make_pair(bp, SECOND)), false_label);
            buffer.bpatch(buffer.makelist(make_pair(br, FIRST)), false_label);
            if (exp1->type_name == "byte" && exp2->type_name == "byte"){ //unsigned
                op = "udiv";
            }
            else{
                op = "sdiv";
            }
            this->store_loc = reg_m.getNewRegister();
            buffer.emit(this->store_loc + " = " + op + " " + typeLLVM + " " + exp1_loc + ", " + exp2_loc);
//            start_line = buffer.emit("br label @ ;start_line");
//            start_label = buffer.genLabel();
        }
    }
    else if (operation == "relop"){
        if (!((exp1->type_name == "int" || exp1->type_name == "byte") &&
            (exp2->type_name == "int" || exp2->type_name == "byte"))) {
            output::errorMismatch(node->line_no);
            exit(0);
        }
//        start_line = buffer.emit("br label @ ;start_line");
//        start_label = buffer.genLabel();
//        buffer.bpatch(buffer.makelist(make_pair(exp1->start_line, FIRST)), exp1->start_label);
//        buffer.bpatch(buffer.makelist(make_pair(exp1->end_line, FIRST)), exp1->end_label);
//        buffer.bpatch(buffer.makelist(make_pair(exp2->start_line, FIRST)), exp2->start_label);
//        buffer.bpatch(buffer.makelist(make_pair(exp2->end_line, FIRST)), exp2->end_label);

        this->type_name = "bool";
        g_exp_type = this->type_name;
        if (exp1->type_name == "byte" && exp2->type_name == "int"){
            string ext_exp1 = reg_m.getNewRegister();
            buffer.emit(ext_exp1 + " = zext i8 " + exp1_loc + " to i32");
            exp1_loc = ext_exp1;
        }
        else if (exp2->type_name == "byte" && exp1->type_name == "int"){
            string ext_exp2 = reg_m.getNewRegister();
            buffer.emit(ext_exp2 + " = zext i8 " + exp2_loc + " to i32");
            exp2_loc = ext_exp2;
        }
        else{
            typeLLVM = LLVMGetType(exp1->type_name);
        }

        if (node->value == "<"){
            op = "slt";
        }
        else if (node->value == "<="){
            op = "sle";
        }
        else if (node->value == ">"){
            op = "sgt";
        }
        else if(node->value == ">="){
            op = "sge";
        }
        else if (node->value == "=="){
            op = "eq";
        }
        else if (node->value == "!="){
            op = "ne";
        }

        this->store_loc = reg_m.getNewRegister();
        buffer.emit(this->store_loc + " = icmp " + op + " " + typeLLVM + " " + exp1_loc + ", " + exp2_loc);
        int br = buffer.emit("br i1 " + this->store_loc + ", label @, label @");
        this->truelist = buffer.makelist(make_pair(br, FIRST));
        this->falselist = buffer.makelist(make_pair(br, SECOND));
//        end_line = buffer.emit("br label @ ;end_line");
//        end_label = buffer.genLabel();
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
Expression::Expression(ASTNode *node, string type_name, string operation, Expression *exp1, Expression *exp2, LabelM *label_m) : ASTNode(type_name, node->line_no){
    if (operation == "and" || operation == "or"){
        if (exp1->type_name != "bool" || exp2->type_name != "bool"){
            output::errorMismatch(node->line_no);
            exit(0);
        }
        this->type_name = "bool";
        g_exp_type = "bool";
        CodeBuffer &buffer = CodeBuffer::instance();
        buffer.emit(";and or");

//        start_line = buffer.emit("br label @ ;start_line");
//        start_label = buffer.genLabel();
//        buffer.bpatch(buffer.makelist(make_pair(exp1->start_line, FIRST)), exp1->start_label);
//        buffer.bpatch(buffer.makelist(make_pair(exp1->end_line, FIRST)), exp1->end_label);
//        buffer.bpatch(buffer.makelist(make_pair(exp2->start_line, FIRST)), exp2->start_label);
//        buffer.bpatch(buffer.makelist(make_pair(exp2->end_line, FIRST)), exp2->end_label);


        if (node->value == "or"){
            buffer.bpatch(exp1->falselist, label_m->label);
            this->truelist = buffer.merge(exp1->truelist, exp2->truelist);
            this->falselist = exp2->falselist;
        }
        else if (node->value == "and"){
            buffer.bpatch(exp1->truelist, label_m->label);
            this->truelist = exp2->truelist;
            this->falselist = buffer.merge(exp1->falselist, exp2->falselist);
        }
//        end_line = buffer.emit("br label @ ;start_line");
//        end_label = buffer.genLabel();
    }
}
/* ExpList Implementation */

ExpList::ExpList(Expression* expression) : ASTNode(expression->value, expression->line_no) {
    if (expression->type_name == "bool"){
        expression->store_loc = getBoolReg(expression, false);
        expression->val_calc();
    }
    exp_list.push_back(expression);
    CodeBuffer &buffer = CodeBuffer::instance();
//    buffer.bpatch(buffer.makelist(make_pair(expression->start_line, FIRST)), expression->start_label);
//    buffer.bpatch(buffer.makelist(make_pair(expression->end_line, FIRST)), expression->end_label);
}

ExpList::ExpList(Expression* expression, ExpList* list) : ASTNode(expression->value, expression->line_no) {
    this->exp_list.push_back(expression);
    this->exp_list.insert(this->exp_list.end(), list->exp_list.begin(), list->exp_list.end());
    CodeBuffer &buffer = CodeBuffer::instance();
//    buffer.bpatch(buffer.makelist(make_pair(expression->start_line, FIRST)), expression->start_label);
//    buffer.bpatch(buffer.makelist(make_pair(expression->end_line, FIRST)), expression->end_label);
}

/* Statement Implementation */

Statement::Statement(ASTNode* statement) : ASTNode("Statement", statement->line_no) {
    CodeBuffer &buffer = CodeBuffer::instance();
    nextlist = statement->nextlist;
    breaklist = statement->breaklist;
    continuelist = statement->continuelist;
    int end_of_statement = buffer.emit("br label @ ;statement"); //according to example in line 42 in bp.cpp
    nextlist.push_back(make_pair(end_of_statement, FIRST));
}

/* Statements Implementation */
Statements::Statements(Statement *statement) : ASTNode("Statements", statement->line_no){
    nextlist = statement->nextlist;
    breaklist = statement->breaklist;
    continuelist = statement->continuelist;
}

Statements::Statements(Statements *statements, LabelM *label_m, Statement *statement) : ASTNode("Statements", statement->line_no){
    CodeBuffer &buffer = CodeBuffer::instance();
    buffer.bpatch(statements->nextlist, label_m->label);
    nextlist = statement->nextlist;
    breaklist = statement->breaklist;
    continuelist = statement->continuelist;
}

/* OpenStatement Implementation */

OpenStatement::OpenStatement(Expression* statement) : ASTNode("OpenStatement", statement->line_no) {}

OpenStatement::OpenStatement(Expression* expression, LabelM* label_m, ASTNode* node) : ASTNode("OpenStatement", expression->line_no) {
    CodeBuffer &buffer = CodeBuffer::instance();
    buffer.bpatch(expression->truelist, label_m->label);
    continuelist = node->continuelist;
    breaklist = node->breaklist;
    nextlist = buffer.merge(expression->falselist, node->nextlist);

//    buffer.bpatch(buffer.makelist(make_pair(expression->start_line, FIRST)), expression->start_label);
//    buffer.bpatch(buffer.makelist(make_pair(expression->end_line, FIRST)), expression->end_label);
}

OpenStatement::OpenStatement(Expression* expression, LabelM* label_m1, ASTNode* node1, ExitM* exit_m, LabelM* label_m2, ASTNode* node2) : ASTNode("OpenStatement", expression->line_no) {
    CodeBuffer &buffer = CodeBuffer::instance();
    buffer.bpatch(expression->truelist, label_m1->label);
    buffer.bpatch(expression->falselist, label_m2->label);
    nextlist = buffer.merge(buffer.merge(node1->nextlist, exit_m->nextlist), node2->nextlist);
    continuelist = buffer.merge(node1->continuelist, node2->continuelist);
    breaklist = buffer.merge(node1->breaklist, node2->breaklist);

//    buffer.bpatch(buffer.makelist(make_pair(expression->start_line, FIRST)), expression->start_label);
//    buffer.bpatch(buffer.makelist(make_pair(expression->end_line, FIRST)), expression->end_label);
}


OpenStatement::OpenStatement(Expression* expression, LabelM* label_m1, LabelM* label_m2, ASTNode* node) : ASTNode("OpenStatement", expression->line_no) {
    CodeBuffer &buffer = CodeBuffer::instance();
    buffer.bpatch(node->nextlist, label_m1->label);
    buffer.bpatch(expression->truelist, label_m2->label);
    nextlist = buffer.merge(expression->falselist, node->breaklist);

    buffer.emit("br label %" + label_m1->label+ ";9");
    buffer.bpatch(node->continuelist, label_m1->label); //TODO: TIMNA-ADD

//    buffer.bpatch(buffer.makelist(make_pair(expression->start_line, FIRST)), expression->start_label);
//    buffer.bpatch(buffer.makelist(make_pair(expression->end_line, FIRST)), expression->end_label);


}


/* ClosedStatement Implementation */

ClosedStatement::ClosedStatement(int line_no) : ASTNode("ClosedStatement", line_no) {}

ClosedStatement::ClosedStatement(SomeStatement* statement) : ASTNode("ClosedStatement", statement->line_no) {
    nextlist = statement->nextlist;
    continuelist = statement->continuelist;
    breaklist = statement->breaklist;
}

ClosedStatement::ClosedStatement(Expression* expression, LabelM* label_m1, ASTNode* node1, ExitM* exit_m, LabelM* label_m2, ASTNode* node2) : ASTNode("ClosedStatement", expression->line_no) {
    CodeBuffer &buffer = CodeBuffer::instance();
    buffer.bpatch(expression->truelist, label_m1->label);
    buffer.bpatch(expression->falselist, label_m2->label);
    nextlist = buffer.merge(buffer.merge(node1->nextlist, exit_m->nextlist), node2->nextlist);
    continuelist = buffer.merge(node1->continuelist, node2->continuelist);
    breaklist = buffer.merge(node1->breaklist, node2->breaklist);

//    buffer.bpatch(buffer.makelist(make_pair(expression->start_line, FIRST)), expression->start_label);
//    buffer.bpatch(buffer.makelist(make_pair(expression->end_line, FIRST)), expression->end_label);


}

ClosedStatement::ClosedStatement(Expression* expression, LabelM* label_m1, LabelM* label_m2, ASTNode* node) : ASTNode("ClosedStatement", expression->line_no) {
    CodeBuffer &buffer = CodeBuffer::instance();
    buffer.bpatch(node->nextlist, label_m1->label);
    buffer.bpatch(expression->truelist, label_m2->label);
    nextlist = buffer.merge(expression->falselist, node->breaklist);
    buffer.bpatch(node->continuelist, label_m1->label);

    buffer.emit("br label %" + label_m1->label + ";1");

//    buffer.bpatch(buffer.makelist(make_pair(expression->start_line, FIRST)), expression->start_label);
//    buffer.bpatch(buffer.makelist(make_pair(expression->end_line, FIRST)), expression->end_label);

}



/* SomeStatement Implementation */

SomeStatement::SomeStatement(Statements* statements, ASTNode *node) : ASTNode("SomeStatement", node->line_no) {
    nextlist = statements->nextlist;
    continuelist = statements->continuelist;
    breaklist = statements->breaklist;
}

SomeStatement::SomeStatement(ASTNode *type, ASTNode *id) : ASTNode("SomeStatement", id->line_no) { //v
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

    CodeBuffer &buffer = CodeBuffer::instance();
    RegisterManager &reg_alloca = RegisterManager::registerAlloc();
    TableEntry entry = tables_stack.back().table_entries_vec.back();
    string reg = reg_alloca.getNewRegister();
    buffer.emit(reg + " = getelementptr [50 x i32], [50 x i32]* " + tables_stack.back().scope_reg + ", i32 0, i32 " +
                                                                                                    to_string(entry.offset) + ";DEBUG1");
    buffer.emit("store i32 0, i32* " + reg);
}


SomeStatement::SomeStatement(ASTNode *type, ASTNode *id, Expression *expression) : ASTNode("SomeStatement", id->line_no) // 16 v
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
        output::errorMismatch(id->line_no);
        exit(0);
    }
    addVarToStack(id->value, type->value);

    CodeBuffer &buffer = CodeBuffer::instance();

//    buffer.bpatch(buffer.makelist(make_pair(expression->start_line, FIRST)), expression->start_label);
//    buffer.bpatch(buffer.makelist(make_pair(expression->end_line, FIRST)), expression->end_label);


    RegisterManager &reg_alloca = RegisterManager::registerAlloc();
    TableEntry entry = tables_stack.back().table_entries_vec.back();
    string ptr_reg = reg_alloca.getNewRegister();
    string expression_reg = reg_alloca.getNewRegister();


    string rh_data_type = getDataTypeRepresentation(rh_type);
    if (expression->is_val_calc) {
        buffer.emit(expression_reg + " = add i1 " + expression->store_loc + ", 0");
    }
    if (rh_data_type == "BOOL") {
        bool_size = true;
        buffer.emit(";sending i1 to getboolreg");
        expression_reg = getBoolReg(expression, bool_size);
    } else if (rh_data_type == "BYTE") {
        if (bool_size)
            buffer.emit(expression_reg + " = zext i8 " + expression->store_loc + " to i32"); //zero extension
        else
            buffer.emit(expression_reg + " = add i8 " + expression->store_loc + ", 0");
    } else if (rh_data_type == "INT") {
        buffer.emit(expression_reg + " = add i32 " + expression->store_loc + ", 0;2"); //add zero
    } else if (rh_data_type == "STRING") {
        expression_reg = expression->store_loc;
    }
    buffer.emit(ptr_reg + " = getelementptr [50 x i32], [50 x i32]* " + tables_stack.back().scope_reg + ", i32 0, i32 " +
                to_string(entry.offset) + ";DEBUG2");
    buffer.emit("store i32 " + expression_reg + ", i32* " + ptr_reg);
}

SomeStatement::SomeStatement(string str, Expression *expression) : ASTNode("SomeStatement", expression->line_no) //kind of v
{
    CodeBuffer &buffer = CodeBuffer::instance();
    RegisterManager &reg_m = RegisterManager::registerAlloc();
    if (str == "return") {
        if (g_return_type == "void" || (!(g_return_type == "int" && expression->type_name == "byte") && g_return_type != expression->type_name)) {

            output::errorMismatch(expression->line_no);
            exit(0);
        }

        string ret_type = LLVMGetType(g_return_type);
        if (ret_type == "void") {
            buffer.emit("ret void");
        } else {
            if (ret_type == "i1") {
                buffer.emit(";DEBUG sending false here!");
                expression->store_loc = getBoolReg(expression, false);
            }
            buffer.emit("ret " + ret_type + " " + expression->store_loc);
        }

//        buffer.bpatch(buffer.makelist(make_pair(expression->start_line, FIRST)), expression->start_label);
//        buffer.bpatch(buffer.makelist(make_pair(expression->end_line, FIRST)), expression->end_label);

    }
    else {
        TableType type;

        if (!varIdTaken(str, &type)) {
            output::errorUndef(expression->line_no, str);
            exit(0);
        }
        TableEntry entry = tables_stack.back().table_entries_vec.back(); //TODO! IS THIS WHAT WE NEED? THE ENTRY'S OFFSET? (AT A FEW LINES DOWN, INSIDE THE EMIT)
        string left_type = type.variable_type;
        string right_type = expression->type_name;
        if (left_type != right_type && !(left_type == "int" && right_type == "byte")) {
            output::errorMismatch(expression->line_no);
            exit(0);
        }
        buffer.emit(";SomeStatement ctor of str+expression is called and exp is not return!");
        string ptr_reg = reg_m.getNewRegister();

//        buffer.bpatch(buffer.makelist(make_pair(expression->start_line, FIRST)), expression->start_label);
//        buffer.bpatch(buffer.makelist(make_pair(expression->end_line, FIRST)), expression->end_label);


        string expression_reg = reg_m.getNewRegister();
        bool_size = true;

        string data_type = getDataTypeRepresentation(expression->type_name);
        if (expression->is_val_calc) {
            buffer.emit(expression_reg + " = add i1 " + expression->store_loc + ", 0");
        }
        if (data_type == "BOOL") {
            buffer.emit(";sending i1 to getboolreg- in somestatement c'tor");
            expression_reg = getBoolReg(expression, bool_size);
        } else if (data_type == "BYTE") {
            if (bool_size)
                buffer.emit(expression_reg + " = zext i8 " + expression->store_loc + " to i32"); //zero extension
            else
                buffer.emit(expression_reg + " = add i8 " + expression->store_loc + ", 0");
        } else if (data_type == "INT") {
            buffer.emit(expression_reg + " = add i32 " + expression->store_loc + ", 0;2"); //add zero
        } else if (data_type == "STRING") {
            expression_reg = expression->store_loc;
        }


        buffer.emit(ptr_reg + " = getelementptr [50 x i32], [50 x i32]* " + tables_stack.back().scope_reg + ", i32 0, i32 " +
                            to_string(entry.offset));
        buffer.emit("store i32 " + expression_reg + ", i32* " + ptr_reg);
    }
}

SomeStatement::SomeStatement(ASTNode *node) : ASTNode("SomeStatement", node->line_no) //v
{
    CodeBuffer &buffer = CodeBuffer::instance();
    if (node->value == "call" || node->value == "{") {
        // Do nothing for "call" or "{"

        //TODO: perhaps need to save truelist and falselist. return here!

        if (dynamic_cast<Expression *>(node)->type_name == "bool" ||
            dynamic_cast<Expression *>(node)->type_name == "BOOL") {
            int tmp_line = buffer.emit("br label @");
            string tmp_lbl = buffer.genLabel();
            buffer.bpatch(dynamic_cast<Expression *>(node)->truelist, tmp_lbl);
            buffer.bpatch(dynamic_cast<Expression *>(node)->falselist, tmp_lbl);
            buffer.bpatch(buffer.makelist(make_pair(tmp_line, FIRST)), tmp_lbl);
        }
//        std::cout << "DEBUG after if clause in SS ctor" << std::endl;
    }
    else if (node->value == "return") {
        if (g_return_type != "VOID") {
            output::errorMismatch(node->line_no);
            exit(0);
        }
        buffer.emit("ret void");
    }
    else if (node->value == "break") {
        if (!tables_stack.back().contains_while_loop) {
            output::errorUnexpectedBreak(node->line_no);
            exit(0);
        }
        int break_line = buffer.emit("br label @ ;2");
        breaklist = buffer.makelist(make_pair(break_line, FIRST));
    }
    else if (node->value == "continue") {
        if (!tables_stack.back().contains_while_loop) {
            output::errorUnexpectedContinue(node->line_no);
            exit(0);
        }
        int cont_line = buffer.emit("br label @ ;3");
        continuelist = buffer.makelist(make_pair(cont_line, FIRST));
    }
}

/*Markers Implementation*/
LabelM::LabelM() : ASTNode("LabelM", -1) {
    CodeBuffer &buffer = CodeBuffer::instance();
    int print_line = buffer.emit("br label @ ;4");
    label = buffer.genLabel();
    buffer.bpatch(buffer.makelist(make_pair(print_line, FIRST)), label);

}

ExitM::ExitM() : ASTNode("ExitM", -1){
    CodeBuffer &buffer = CodeBuffer::instance();
    int print_line = buffer.emit("br label @; exit");
    nextlist = buffer.makelist(make_pair(print_line, FIRST));
}

/* Auxiliaries Implementation */

string getBoolReg(Expression* expression, bool bool_size) {
    CodeBuffer &buffer = CodeBuffer::instance();
    RegisterManager &reg_alloca = RegisterManager::registerAlloc();
    string size;
    if (bool_size)
        size = "i32";
    else
        size = "i1";
    int bp_start = buffer.emit("br label @ ;5");
    string reg_true_val = reg_alloca.getNewRegister();
    string label_true_val = buffer.genLabel();
    buffer.emit(reg_true_val + " = add " + size + " 1, 0");
    int bp_fin_1 = buffer.emit("br label @ ;6");

    string reg_false_val = reg_alloca.getNewRegister();
    string label_false_val = buffer.genLabel();
    buffer.emit(reg_false_val + " = add " + size + " 0, 0");
    int bp_fin_2 = buffer.emit("br label @ ;7");


    string reg_final = reg_alloca.getNewRegister();

    string label_final = buffer.genLabel();

    buffer.emit(reg_final + " = phi " + size + "[1, %" + label_true_val + "], [0, %" + label_false_val + "]");

    buffer.bpatch(expression->truelist, label_true_val);
    buffer.bpatch(expression->falselist, label_false_val);

    buffer.bpatch(buffer.makelist(make_pair(bp_start, FIRST)), label_true_val);


    buffer.bpatch(buffer.makelist(make_pair(bp_fin_1, FIRST)), label_final);

    buffer.bpatch(buffer.makelist(make_pair(bp_fin_2, FIRST)), label_final);

    return reg_final;
}

void validateMain(){
  //  std::cout<< "DEBUG validateMain" <<std::endl;
    Table top_table = tables_stack[0];
    int size = top_table.table_entries_vec.size();
    TableEntry current_entry;
    for (int i = 0; i < size; i++){
        current_entry = top_table.table_entries_vec[i];
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
    offsets_stack.push_back(offsets_stack.back()); //try2
    Table t;
    t.contains_while_loop = tables_stack.back().contains_while_loop;
    t.scope_reg = tables_stack.back().scope_reg;
    tables_stack.push_back(t);


}

void newWhileScope()
{
    offsets_stack.push_back(offsets_stack.back()); //try1
    Table t;
    t.contains_while_loop = true;
    t.scope_reg = tables_stack.back().scope_reg;
    tables_stack.push_back(t);

}


void isBool(int line_number)
{
    if (g_exp_type.compare("bool") != 0)
    {
        output::errorMismatch(line_number);
        exit(0);
    }
}


bool varIdTaken(string id_name, TableType *id_type){
    Table *current_table;
    for (int i = tables_stack.size()-1; i > 0; i--) {
        current_table = &tables_stack[i];
        for (int j = 0; j < current_table->table_entries_vec.size(); j++) {
            TableEntry *entry = &current_table->table_entries_vec[j];
            if (entry->name == id_name) {
                if (id_type != nullptr) {
                    *id_type = entry->type;
                }
                return true;
            }
        }
    }
    return false;
}

void printScopesSymbols(){
    output::endScope();
    Table *curr_scope = &tables_stack.back();
    for (int i=0; i<curr_scope->table_entries_vec.size(); i++){
        TableEntry *curr_entry = &curr_scope->table_entries_vec[i];
        if (curr_entry->type.function_type){
            vector<string> args;
            if (curr_entry->type.func_decl->arg_types.size() != 0){
                for (string type : curr_entry->type.func_decl->arg_types) {
                    args.push_back(getDataTypeRepresentation(type));
                }
            }
            output::printID(curr_entry->name, curr_entry->offset, output::makeFunctionType(getDataTypeRepresentation(curr_entry->type.func_decl->ret_type_str), args));
        }
        else {
            output::printID(curr_entry->name, curr_entry->offset, getDataTypeRepresentation(curr_entry->type.variable_type));
        }
    }
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
                        for (int i=0; i< explist->exp_list.size() ; i++){
                            if (current_entry.type.func_decl->arg_types.at(i) != explist->exp_list.at(i)->type_name){
                                if (!((current_entry.type.func_decl->arg_types.at(i) == "int" && explist->exp_list.at(i)->type_name == "byte"))){
                                    match = false;


                                }
                            }
                        }
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
                        if (*found) {
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
