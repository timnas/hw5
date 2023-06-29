%{
/* Declarations section */
#include "class_defs.hpp"
#include "hw3_output.hpp"
#include "parser.tab.hpp"
#include <iostream>

const int BUFFER_SIZE = 1024;
char str[BUFFER_SIZE];
int global_i;
%}

%option noyywrap
%option yylineno

digit       ([0-9])
letter      ([a-zA-Z])
int         (int)
void        (void)
byte        (byte)
b           (b)
bool        (bool)
override    (override)
and         (and)
or          (or)
not         (not)
true        (true)
false       (false)
return      (return)
if          (if)
else        (else)
while       (while)
break       (break)
continue    (continue)
sc          (;)
comma       (,)
lparen      (\()
rparen      (\))
lbrace      (\{)
rbrace      (\})
assign      (=)
relop       (!=|==|[<>]=?|<>)
mul         ("*")
div         ("/")
add         ("+")
sub         ("-")
num         ([1-9]{digit}*)|0


%%
{void}                          {yylval = new ASTNode(yytext, yylineno); return VOID;}
{int}                           {yylval = new ASTNode(yytext, yylineno); return INT;}
{byte}                          {yylval = new ASTNode(yytext, yylineno); return BYTE;}
{b}                             {yylval = new ASTNode(yytext, yylineno); return B;}
{bool}                          {yylval = new ASTNode(yytext, yylineno); return BOOL;}
{override}                      {yylval = new ASTNode(yytext, yylineno); return OVERRIDE;}
{and}                           {yylval = new ASTNode(yytext, yylineno); return AND;}
{or}                            {yylval = new ASTNode(yytext, yylineno); return OR;}
{not}                           {yylval = new ASTNode(yytext, yylineno); return NOT;}
{true}                          {yylval = new ASTNode(yytext, yylineno); return TRUE;}
{false}                         {yylval = new ASTNode(yytext, yylineno); return FALSE;}
{return}                        {yylval = new ASTNode(yytext, yylineno); return RETURN;}
{if}                            {yylval = new ASTNode(yytext, yylineno); return IF;}
{else}                          {yylval = new ASTNode(yytext, yylineno); return ELSE;}
{while}                         {yylval = new ASTNode(yytext, yylineno); return WHILE;}
{break}                         {yylval = new ASTNode(yytext, yylineno); return BREAK;}
{continue}                      {yylval = new ASTNode(yytext, yylineno); return CONTINUE;}
{sc}                            {yylval = new ASTNode(yytext, yylineno); return SC;}
{comma}                         {yylval = new ASTNode(yytext, yylineno); return COMMA;}
{lparen}                        {yylval = new ASTNode(yytext, yylineno); return LPAREN;}
{rparen}                        {yylval = new ASTNode(yytext, yylineno); return RPAREN;}
{lbrace}                        {yylval = new ASTNode(yytext, yylineno); return LBRACE;}
{rbrace}                        {yylval = new ASTNode(yytext, yylineno); return RBRACE;}
{assign}                        {yylval = new ASTNode(yytext, yylineno); return ASSIGN;}
{relop}                         {yylval = new ASTNode(yytext, yylineno); return RELOP;}
{mul}                           {yylval = new ASTNode(yytext, yylineno); return MUL;}
{div}                           {yylval = new ASTNode(yytext, yylineno); return DIV;}
{add}                           {yylval = new ASTNode(yytext, yylineno); return ADD;}
{sub}                           {yylval = new ASTNode(yytext, yylineno); return SUB;}
{letter}+({letter}|{digit})*    {yylval = new ASTNode(yytext, yylineno); return ID;}
{num}                           {yylval = new ASTNode(yytext, yylineno);return NUM;}
\"([^\n\r\"\\]|\\[rnt"\\])+\"   {yylval = new ASTNode(yytext, yylineno); return STRING;}
[ \n\r\t]                       ;
"//"[^\r\n]*[\r|\n|\r\n]?       ;                                    // comment- do nothing
.                               output::errorLex(yylineno); exit(0); // all the rest: error from output file

%%
/* no functions needed*/