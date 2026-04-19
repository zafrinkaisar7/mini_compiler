%{
#include "parser.tab.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int header_done = 0;
static void tok_out(const char* cat, const char* text){
    if(!header_done){
        printf("===============================================================\n");
        printf("TOKENIZATION\n\n\n");
        header_done = 1;
    }
    printf("%s: %s\n", cat, text);
}
%}

%option noyywrap
%option yylineno

%%

int                 { tok_out("Keyword", "int");   return INT; }
if                  { tok_out("Keyword", "if");    return IF; }
else                { tok_out("Keyword", "else");  return ELSE; }
while               { tok_out("Keyword", "while"); return WHILE; }
print               { tok_out("Keyword", "print"); return PRINT; }

"=="                { tok_out("Relational/Assignment Operator", "=="); return EQ; }
"!="                { tok_out("Relational/Assignment Operator", "!="); return NE; }
">="                { tok_out("Relational/Assignment Operator", ">="); return GE; }
"<="                { tok_out("Relational/Assignment Operator", "<="); return LE; }
">"                 { tok_out("Relational/Assignment Operator", ">");  return GT; }
"<"                 { tok_out("Relational/Assignment Operator", "<");  return LT; }
"="                 { tok_out("Relational/Assignment Operator", "=");  return '='; }

"+"                 { tok_out("Arithmetic Operator", "+");  return '+'; }
"-"                 { tok_out("Arithmetic Operator", "-");  return '-'; }
"*"                 { tok_out("Arithmetic Operator", "*");  return '*'; }
"/"                 { tok_out("Arithmetic Operator", "/");  return '/'; }

";"                 { tok_out("Punctuation",  ";"); return ';'; }
"("                 { tok_out("Parenthesis", "(");  return '('; }
")"                 { tok_out("Parenthesis", ")");  return ')'; }
"{"                 { tok_out("Braces",  "{"); return '{'; }
"}"                 { tok_out("Braces",  "}"); return '}'; }

[A-Za-z][A-Za-z0-9_]* {
    tok_out("Identifier", yytext);
    strcpy(yylval.str, yytext);
    return ID;
}

[0-9]+              {
    tok_out("Constant", yytext);
    yylval.num = atoi(yytext);
    return NUMBER;
}

[ \t\r\n]+          ;

.                   { tok_out("Invalid", yytext); }

%%

