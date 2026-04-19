/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 1 "parser.y"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern int yylex(void);
void yyerror(const char *s);
extern int yylineno;

/* ---------------- Symbol Table ---------------- */
#define HASHSIZE 100

struct Symbol {
    char name[100];
    int offset;   /* rbp-relative */
    int value;    /* for interpreter */
    struct Symbol *next;
};

static struct Symbol *hashtable[HASHSIZE] = {0};

static int hash(char *name) {
    int h = 0;
    for (char *p = name; *p; p++) h = h * 31 + (unsigned char)*p;
    return (h < 0 ? -h : h) % HASHSIZE;
}

static struct Symbol *lookup(char *name) {
    int h = hash(name);
    for (struct Symbol *s = hashtable[h]; s; s = s->next)
        if (strcmp(s->name, name) == 0) return s;
    return NULL;
}

static void insert(char *name, int offset) {
    int h = hash(name);
    struct Symbol *s = (struct Symbol*)malloc(sizeof(struct Symbol));
    strcpy(s->name, name);
    s->offset = offset;
    s->value = 0;
    s->next = hashtable[h];
    hashtable[h] = s;
}

/* ---------------- AST Nodes ---------------- */
struct node {
    struct node *left;
    struct node *right;
    char token[100];  /* "#": number, otherwise operator/ID/keyword */
    int num;          /* number value if token == "#" */
};

static struct node *mknode(struct node *left, struct node *right, const char *token) {
    struct node *n = (struct node*)malloc(sizeof(struct node));
    n->left = left; n->right = right;
    strncpy(n->token, token, sizeof(n->token)-1);
    n->token[sizeof(n->token)-1] = 0;
    n->num = 0;
    return n;
}

static struct node *mkconst(int val) {
    struct node *n = mknode(NULL, NULL, "#");
    n->num = val;
    return n;
}

static struct node *head = NULL;
static int var_count = 0;

/* ---------------- TAC generation ---------------- */
static char icg[1000][100];
static int ic_idx = 0;
static int temp_idx = 0;
static int label_idx = 0;

static void reset_codegen(void){ ic_idx=0; temp_idx=0; label_idx=0; }

static char *new_temp(void) {
    char *buf = (char*)malloc(16);
    sprintf(buf, "t%d", temp_idx++);
    return buf;
}
static char *new_label(void) {
    char *buf = (char*)malloc(16);
    sprintf(buf, "L%d", label_idx++);
    return buf;
}

static char *gen_expr(struct node *n) {
    if (!n->left && !n->right) {
        if (n->token[0] == '#') {
            char *t = new_temp();
            sprintf(icg[ic_idx++], "%s = %d", t, n->num);
            return t;
        } else {
            if (!lookup(n->token)) yyerror("Undeclared variable");
            char *t = new_temp();
            sprintf(icg[ic_idx++], "%s = %s", t, n->token);
            return t;
        }
    } else {
        char *l = gen_expr(n->left);
        char *r = gen_expr(n->right);
        char *t = new_temp();
        sprintf(icg[ic_idx++], "%s = %s %s %s", t, l, n->token, r);
        free(l); free(r);
        return t;
    }
}

static void gen_assign(char *id, struct node *expr) {
    char *rhs = gen_expr(expr);
    sprintf(icg[ic_idx++], "%s = %s", id, rhs);
    free(rhs);
}

struct cond_info { char tt[16]; char ff[16]; };

static struct cond_info gen_cond(struct node *n) {
    char *l = gen_expr(n->left);
    char *r = gen_expr(n->right);
    struct cond_info ci;
    strcpy(ci.tt, new_label());
    strcpy(ci.ff, new_label());
    sprintf(icg[ic_idx++], "IF %s %s %s GOTO %s", l, n->token, r, ci.tt);
    sprintf(icg[ic_idx++], "GOTO %s", ci.ff);
    free(l); free(r);
    return ci;
}

static void gen_statements(struct node *stmts); /* fwd */

static void gen_if(struct node *cond, struct node *thenBlk, struct node *elseBlk) {
    struct cond_info ci = gen_cond(cond);
    sprintf(icg[ic_idx++], "%s:", ci.tt);
    gen_statements(thenBlk);
    char *end = new_label();
    sprintf(icg[ic_idx++], "GOTO %s", end);
    sprintf(icg[ic_idx++], "%s:", ci.ff);
    if (elseBlk) gen_statements(elseBlk);
    sprintf(icg[ic_idx++], "%s:", end);
    free(end);
}

static void gen_while(struct node *cond, struct node *body) {
    char *begin = new_label();
    sprintf(icg[ic_idx++], "%s:", begin);
    struct cond_info ci = gen_cond(cond);
    sprintf(icg[ic_idx++], "%s:", ci.tt);
    gen_statements(body);
    sprintf(icg[ic_idx++], "GOTO %s", begin);
    sprintf(icg[ic_idx++], "%s:", ci.ff);
    free(begin);
}

static void gen_print(struct node *expr) {
    char *t = gen_expr(expr);
    sprintf(icg[ic_idx++], "PRINT %s", t);
    free(t);
}

static void gen_statement(struct node *stmt) {
    if (!stmt) return;
    if (strcmp(stmt->token, "assign") == 0)       gen_assign(stmt->left->token, stmt->right);
    else if (strcmp(stmt->token, "if") == 0)      gen_if(stmt->left, stmt->right->left, stmt->right->right);
    else if (strcmp(stmt->token, "while") == 0)   gen_while(stmt->left, stmt->right);
    else if (strcmp(stmt->token, "print") == 0)   gen_print(stmt->left);
}
static void gen_statements(struct node *stmts) { if (stmts){ gen_statement(stmts->left); gen_statements(stmts->right);} }

static void print_icg(void) {
    printf("\nIntermediate Code (TAC):\n");
    for (int i=0;i<ic_idx;i++) printf("%s\n", icg[i]);
}

/* -------- Simple Assembly (human-readable) -------- */
static char* expr_str(struct node *n){
    if(!n->left && !n->right){
        if(n->token[0]=='#'){ char *s=(char*)malloc(32); sprintf(s,"%d",n->num); return s; }
        char *s=(char*)malloc(strlen(n->token)+1); strcpy(s,n->token); return s;
    }
    char *L=expr_str(n->left), *R=expr_str(n->right);
    size_t len=strlen(L)+strlen(R)+4;
    char *s=(char*)malloc(len+1);
    sprintf(s,"(%s %s %s)",L,n->token,R);
    free(L); free(R);
    return s;
}
static char* cond_str(struct node *n){
    char *L=expr_str(n->left), *R=expr_str(n->right);
    size_t len=strlen(L)+strlen(R)+4;
    char *s=(char*)malloc(len+1);
    sprintf(s,"(%s %s %s)",L,n->token,R);
    free(L); free(R);
    return s;
}

static void sa_statements(struct node *n); /* fwd */

static void sa_assign(char *id, struct node *expr){
    char *e=expr_str(expr);
    printf("MOV %s, %s\n", id, e);
    free(e);
}
static void sa_if(struct node *cond, struct node *thenBlk, struct node *elseBlk){
    char *Ltrue=new_label(), *Lfalse=new_label(), *Lend=new_label();
    char *c=cond_str(cond);
    printf("IF %s GOTO %s\n", c, Ltrue);
    printf("GOTO %s\n", Lfalse);
    printf("%s:\n", Ltrue);
    sa_statements(thenBlk);
    printf("GOTO %s\n", Lend);
    printf("%s:\n", Lfalse);
    if(elseBlk) sa_statements(elseBlk);
    printf("%s:\n", Lend);
    free(c); free(Ltrue); free(Lfalse); free(Lend);
}
static void sa_while(struct node *cond, struct node *body){
    char *Lbegin=new_label(), *Ltrue=new_label(), *Lfalse=new_label();
    printf("%s:\n", Lbegin);
    char *c=cond_str(cond);
    printf("IF %s GOTO %s\n", c, Ltrue);
    printf("GOTO %s\n", Lfalse);
    printf("%s:\n", Ltrue);
    sa_statements(body);
    printf("GOTO %s\n", Lbegin);
    printf("%s:\n", Lfalse);
    free(c); free(Lbegin); free(Ltrue); free(Lfalse);
}
static void sa_print(struct node *expr){
    char *e=expr_str(expr);
    printf("PRINT %s\n", e);
    free(e);
}
static void sa_statement(struct node *stmt){
    if(!stmt) return;
    if (strcmp(stmt->token,"assign")==0)       sa_assign(stmt->left->token, stmt->right);
    else if (strcmp(stmt->token,"if")==0)      sa_if(stmt->left, stmt->right->left, stmt->right->right);
    else if (strcmp(stmt->token,"while")==0)   sa_while(stmt->left, stmt->right);
    else if (strcmp(stmt->token,"print")==0)   sa_print(stmt->left);
}
static void sa_statements(struct node *n){ if(n){ sa_statement(n->left); sa_statements(n->right);} }

static void print_simple_assembly(void){
    printf("\nSimple Assembly:\n");
    /* locals list */
    printf(".locals");
    int first=1;
    for(int i=0;i<HASHSIZE;i++){
        for(struct Symbol *s=hashtable[i]; s; s=s->next){
            printf("%s %s", first? "": ",", s->name); first=0;
        }
    }
    printf("\n\n");
    label_idx = 0; /* make labels start from L0 here */
    sa_statements(head);
    printf("HALT\n");
}

/* -------- x86-64 GAS (AT&T) -------- */
static int framesize = 0;

static void gen_assembly_header(FILE *f) {
    fprintf(f, ".data\nfmt: .string \"%%d\\n\"\n.text\n.global main\nmain:\n\tpushq %%rbp\n\tmovq %%rsp, %%rbp\n");
    framesize = ((var_count * 8 + 15) / 16) * 16;
    if (framesize > 0) fprintf(f, "\tsubq $%d, %%rsp\n", framesize);
}
static void gen_assembly_footer(FILE *f) {
    fprintf(f, "\tmovq $0, %%rax\n\tmovq %%rbp, %%rsp\n\tpopq %%rbp\n\tret\n");
}
static void gen_assembly_expr(struct node *n, FILE *f) {
    if (!n->left && !n->right) {
        if (n->token[0] == '#') fprintf(f, "\tpushq $%d\n", n->num);
        else {
            struct Symbol *s = lookup(n->token);
            if(!s){ fprintf(stderr,"ICE: unknown id %s\n", n->token); exit(1); }
            fprintf(f, "\tpushq %d(%%rbp)\n", s->offset);
        }
        return;
    }
    gen_assembly_expr(n->left, f);
    gen_assembly_expr(n->right, f);
    fprintf(f, "\tpopq %%rbx\n\tpopq %%rax\n");
    if      (strcmp(n->token, "+")==0) fprintf(f, "\taddq %%rbx, %%rax\n");
    else if (strcmp(n->token, "-")==0) fprintf(f, "\tsubq %%rbx, %%rax\n");
    else if (strcmp(n->token, "*")==0) fprintf(f, "\timulq %%rbx, %%rax\n");
    else if (strcmp(n->token, "/")==0) { fprintf(f, "\tcqo\n\tidivq %%rbx\n"); }
    fprintf(f, "\tpushq %%rax\n");
}
static void gen_assembly_assign(char *id, struct node *expr, FILE *f) {
    gen_assembly_expr(expr, f);
    struct Symbol *s = lookup(id);
    fprintf(f, "\tpopq %%rax\n\tmovq %%rax, %d(%%rbp)\n", s->offset);
}
struct asm_cond_info { char tt[16]; char ff[16]; char op[8]; };
static struct asm_cond_info gen_assembly_cond(struct node *n, FILE *f) {
    gen_assembly_expr(n->left, f);
    gen_assembly_expr(n->right, f);
    fprintf(f, "\tpopq %%rbx\n\tpopq %%rax\n\tcmpq %%rbx, %%rax\n");
    struct asm_cond_info ci;
    strcpy(ci.tt, new_label());
    strcpy(ci.ff, new_label());
    strncpy(ci.op, n->token, sizeof(ci.op)-1); ci.op[sizeof(ci.op)-1]=0;
    return ci;
}
static void gen_assembly_statements(struct node *n, FILE *f); /* fwd */
static void gen_assembly_if(struct node *cond, struct node *thenBlk, struct node *elseBlk, FILE *f) {
    struct asm_cond_info ci = gen_assembly_cond(cond, f);
    const char *jmp = "jne";
    if      (strcmp(ci.op,"==")==0) jmp="je";
    else if (strcmp(ci.op,"!=")==0) jmp="jne";
    else if (strcmp(ci.op,">" )==0) jmp="jg";
    else if (strcmp(ci.op,">=")==0) jmp="jge";
    else if (strcmp(ci.op,"<" )==0) jmp="jl";
    else if (strcmp(ci.op,"<=")==0) jmp="jle";
    fprintf(f, "\t%s %s\n", jmp, ci.tt);
    fprintf(f, "\tjmp %s\n", ci.ff);
    fprintf(f, "%s:\n", ci.tt);
    gen_assembly_statements(thenBlk, f);
    char *end = new_label();
    fprintf(f, "\tjmp %s\n", end);
    fprintf(f, "%s:\n", ci.ff);
    if (elseBlk) gen_assembly_statements(elseBlk, f);
    fprintf(f, "%s:\n", end);
    free(end);
}
static void gen_assembly_while(struct node *cond, struct node *body, FILE *f) {
    char *begin = new_label();
    fprintf(f, "%s:\n", begin);
    struct asm_cond_info ci = gen_assembly_cond(cond, f);
    const char *jmp = "jne";
    if      (strcmp(ci.op,"==")==0) jmp="je";
    else if (strcmp(ci.op,"!=")==0) jmp="jne";
    else if (strcmp(ci.op,">" )==0) jmp="jg";
    else if (strcmp(ci.op,">=")==0) jmp="jge";
    else if (strcmp(ci.op,"<" )==0) jmp="jl";
    else if (strcmp(ci.op,"<=")==0) jmp="jle";
    fprintf(f, "\t%s %s\n", jmp, ci.tt);
    fprintf(f, "\tjmp %s\n", ci.ff);
    fprintf(f, "%s:\n", ci.tt);
    gen_assembly_statements(body, f);
    fprintf(f, "\tjmp %s\n", begin);
    fprintf(f, "%s:\n", ci.ff);
    free(begin);
}
static void gen_assembly_print(struct node *expr, FILE *f) {
    gen_assembly_expr(expr, f);
    fprintf(f, "\tpopq %%rsi\n\tmovq $fmt, %%rdi\n\tmovq $0, %%rax\n\tcall printf\n");
}
static void gen_assembly_statement(struct node *stmt, FILE *f) {
    if (!stmt) return;
    if (strcmp(stmt->token,"assign")==0)       gen_assembly_assign(stmt->left->token, stmt->right, f);
    else if (strcmp(stmt->token,"if")==0)      gen_assembly_if(stmt->left, stmt->right->left, stmt->right->right, f);
    else if (strcmp(stmt->token,"while")==0)   gen_assembly_while(stmt->left, stmt->right, f);
    else if (strcmp(stmt->token,"print")==0)   gen_assembly_print(stmt->left, f);
}
static void gen_assembly_statements(struct node *n, FILE *f) { if(n){ gen_assembly_statement(n->left,f); gen_assembly_statements(n->right,f);} }
static void print_gas_assembly(void) {
    printf("\nAssembly Code (x86):\n");
    FILE *f = stdout;
    gen_assembly_header(f);
    gen_assembly_statements(head, f);
    gen_assembly_footer(f);
}

/* ---------------- Interpreter ---------------- */
static int eval_expr(struct node *n) {
    if (!n->left && !n->right) {
        if (n->token[0]=='#') return n->num;
        struct Symbol *s = lookup(n->token);
        return s ? s->value : 0;
    }
    int L = eval_expr(n->left), R = eval_expr(n->right);
    if      (strcmp(n->token,"+")==0) return L+R;
    else if (strcmp(n->token,"-")==0) return L-R;
    else if (strcmp(n->token,"*")==0) return L*R;
    else if (strcmp(n->token,"/")==0) return L/R;
    return 0;
}
static int eval_cond(struct node *n) {
    int L = eval_expr(n->left), R = eval_expr(n->right);
    if      (strcmp(n->token,"==")==0) return L==R;
    else if (strcmp(n->token,"!=")==0) return L!=R;
    else if (strcmp(n->token,">" )==0) return L> R;
    else if (strcmp(n->token,">=")==0) return L>=R;
    else if (strcmp(n->token,"<" )==0) return L< R;
    else if (strcmp(n->token,"<=")==0) return L<=R;
    return 0;
}
static void exec_statements(struct node *n);
static void exec_statement(struct node *stmt) {
    if (!stmt) return;
    if (strcmp(stmt->token,"decl")==0) {
    } else if (strcmp(stmt->token,"assign")==0) {
        int v = eval_expr(stmt->right);
        struct Symbol *s = lookup(stmt->left->token);
        s->value = v;
    } else if (strcmp(stmt->token,"if")==0) {
        if (eval_cond(stmt->left)) exec_statements(stmt->right->left);
        else if (stmt->right->right) exec_statements(stmt->right->right);
    } else if (strcmp(stmt->token,"while")==0) {
        while (eval_cond(stmt->left)) exec_statements(stmt->right);
    } else if (strcmp(stmt->token,"print")==0) {
        printf("%d\n", eval_expr(stmt->left));
    }
}
static void exec_statements(struct node *n) { if(n){ exec_statement(n->left); exec_statements(n->right);} }
static void print_program_output(void) { printf("\nProgram Output:\n"); exec_statements(head); }

#line 483 "parser.tab.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "parser.tab.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_ID = 3,                         /* ID  */
  YYSYMBOL_NUMBER = 4,                     /* NUMBER  */
  YYSYMBOL_INT = 5,                        /* INT  */
  YYSYMBOL_IF = 6,                         /* IF  */
  YYSYMBOL_ELSE = 7,                       /* ELSE  */
  YYSYMBOL_WHILE = 8,                      /* WHILE  */
  YYSYMBOL_PRINT = 9,                      /* PRINT  */
  YYSYMBOL_EQ = 10,                        /* EQ  */
  YYSYMBOL_NE = 11,                        /* NE  */
  YYSYMBOL_GT = 12,                        /* GT  */
  YYSYMBOL_LT = 13,                        /* LT  */
  YYSYMBOL_GE = 14,                        /* GE  */
  YYSYMBOL_LE = 15,                        /* LE  */
  YYSYMBOL_16_ = 16,                       /* '+'  */
  YYSYMBOL_17_ = 17,                       /* '-'  */
  YYSYMBOL_18_ = 18,                       /* '*'  */
  YYSYMBOL_19_ = 19,                       /* '/'  */
  YYSYMBOL_20_ = 20,                       /* ';'  */
  YYSYMBOL_21_ = 21,                       /* '='  */
  YYSYMBOL_22_ = 22,                       /* '('  */
  YYSYMBOL_23_ = 23,                       /* ')'  */
  YYSYMBOL_24_ = 24,                       /* '{'  */
  YYSYMBOL_25_ = 25,                       /* '}'  */
  YYSYMBOL_YYACCEPT = 26,                  /* $accept  */
  YYSYMBOL_program = 27,                   /* program  */
  YYSYMBOL_statements = 28,                /* statements  */
  YYSYMBOL_statement = 29,                 /* statement  */
  YYSYMBOL_decl = 30,                      /* decl  */
  YYSYMBOL_assign = 31,                    /* assign  */
  YYSYMBOL_expr = 32,                      /* expr  */
  YYSYMBOL_term = 33,                      /* term  */
  YYSYMBOL_factor = 34,                    /* factor  */
  YYSYMBOL_cond = 35,                      /* cond  */
  YYSYMBOL_if_stmt = 36,                   /* if_stmt  */
  YYSYMBOL_while_stmt = 37,                /* while_stmt  */
  YYSYMBOL_print_stmt = 38                 /* print_stmt  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  24
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   62

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  26
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  13
/* YYNRULES -- Number of rules.  */
#define YYNRULES  30
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  67

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   270


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      22,    23,    18,    16,     2,    17,     2,    19,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    20,
       2,    21,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    24,     2,    25,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   428,   428,   431,   432,   436,   437,   438,   439,   440,
     444,   454,   463,   464,   465,   469,   470,   471,   475,   476,
     477,   481,   482,   483,   484,   485,   486,   490,   491,   495,
     499
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "ID", "NUMBER", "INT",
  "IF", "ELSE", "WHILE", "PRINT", "EQ", "NE", "GT", "LT", "GE", "LE",
  "'+'", "'-'", "'*'", "'/'", "';'", "'='", "'('", "')'", "'{'", "'}'",
  "$accept", "program", "statements", "statement", "decl", "assign",
  "expr", "term", "factor", "cond", "if_stmt", "while_stmt", "print_stmt", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-16)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int8 yypact[] =
{
       9,   -15,     6,    -9,    23,    -1,    47,   -16,     9,   -16,
     -16,   -16,   -16,   -16,    -1,    28,    -1,    -1,   -16,   -16,
      -1,    24,   -11,   -16,   -16,   -16,    26,   -16,    12,    29,
      30,   -12,    -1,    -1,   -16,    -1,    -1,   -16,    -1,    -1,
      -1,    -1,    -1,    -1,    27,    31,   -16,   -11,   -11,   -16,
     -16,     3,     3,     3,     3,     3,     3,     9,     9,    32,
      33,    52,   -16,    36,     9,    37,   -16
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       4,     0,     0,     0,     0,     0,     0,     2,     4,     5,
       6,     7,     8,     9,     0,     0,     0,     0,    19,    20,
       0,     0,    14,    17,     1,     3,     0,    10,     0,     0,
       0,     0,     0,     0,    30,     0,     0,    11,     0,     0,
       0,     0,     0,     0,     0,     0,    18,    12,    13,    15,
      16,    21,    22,    23,    24,    25,    26,     4,     4,     0,
       0,    27,    29,     0,     4,     0,    28
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -16,   -16,    -8,   -16,   -16,   -16,    -4,    -2,    -3,    44,
     -16,   -16,   -16
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
       0,     6,     7,     8,     9,    10,    28,    22,    23,    29,
      11,    12,    13
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int8 yytable[] =
{
      25,    21,    18,    19,    32,    33,    14,    35,    36,    15,
      26,    46,     1,    16,     2,     3,    31,     4,     5,    32,
      33,    20,    38,    39,    40,    41,    42,    43,    32,    33,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      32,    33,    32,    33,    34,    17,    37,    24,    27,    59,
      60,    57,    44,    45,     0,    58,    65,    61,    62,    63,
      64,    30,    66
};

static const yytype_int8 yycheck[] =
{
       8,     5,     3,     4,    16,    17,    21,    18,    19,     3,
      14,    23,     3,    22,     5,     6,    20,     8,     9,    16,
      17,    22,    10,    11,    12,    13,    14,    15,    16,    17,
      32,    33,    35,    36,    38,    39,    40,    41,    42,    43,
      16,    17,    16,    17,    20,    22,    20,     0,    20,    57,
      58,    24,    23,    23,    -1,    24,    64,    25,    25,     7,
      24,    17,    25
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     3,     5,     6,     8,     9,    27,    28,    29,    30,
      31,    36,    37,    38,    21,     3,    22,    22,     3,     4,
      22,    32,    33,    34,     0,    28,    32,    20,    32,    35,
      35,    32,    16,    17,    20,    18,    19,    20,    10,    11,
      12,    13,    14,    15,    23,    23,    23,    33,    33,    34,
      34,    32,    32,    32,    32,    32,    32,    24,    24,    28,
      28,    25,    25,     7,    24,    28,    25
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    26,    27,    28,    28,    29,    29,    29,    29,    29,
      30,    31,    32,    32,    32,    33,    33,    33,    34,    34,
      34,    35,    35,    35,    35,    35,    35,    36,    36,    37,
      38
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     2,     0,     1,     1,     1,     1,     1,
       3,     4,     3,     3,     1,     3,     3,     1,     3,     1,
       1,     3,     3,     3,     3,     3,     3,     7,    11,     7,
       3
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* program: statements  */
#line 428 "parser.y"
                     { head = (yyvsp[0].nd); (yyval.nd) = (yyvsp[0].nd); }
#line 1540 "parser.tab.c"
    break;

  case 3: /* statements: statement statements  */
#line 431 "parser.y"
                           { (yyval.nd) = mknode((yyvsp[-1].nd), (yyvsp[0].nd), "statements"); }
#line 1546 "parser.tab.c"
    break;

  case 4: /* statements: %empty  */
#line 432 "parser.y"
                           { (yyval.nd) = NULL; }
#line 1552 "parser.tab.c"
    break;

  case 5: /* statement: decl  */
#line 436 "parser.y"
                   { (yyval.nd) = (yyvsp[0].nd); }
#line 1558 "parser.tab.c"
    break;

  case 6: /* statement: assign  */
#line 437 "parser.y"
                   { (yyval.nd) = (yyvsp[0].nd); }
#line 1564 "parser.tab.c"
    break;

  case 7: /* statement: if_stmt  */
#line 438 "parser.y"
                   { (yyval.nd) = (yyvsp[0].nd); }
#line 1570 "parser.tab.c"
    break;

  case 8: /* statement: while_stmt  */
#line 439 "parser.y"
                   { (yyval.nd) = (yyvsp[0].nd); }
#line 1576 "parser.tab.c"
    break;

  case 9: /* statement: print_stmt  */
#line 440 "parser.y"
                   { (yyval.nd) = (yyvsp[0].nd); }
#line 1582 "parser.tab.c"
    break;

  case 10: /* decl: INT ID ';'  */
#line 445 "parser.y"
      {
        if (lookup((yyvsp[-1].str))) yyerror("Redeclaration");
        insert((yyvsp[-1].str), -8 * (++var_count));  /* negative offsets: safe */
        struct node *idnode = mknode(NULL, NULL, (yyvsp[-1].str));
        (yyval.nd) = mknode(idnode, NULL, "decl");
      }
#line 1593 "parser.tab.c"
    break;

  case 11: /* assign: ID '=' expr ';'  */
#line 455 "parser.y"
      {
        if (!lookup((yyvsp[-3].str))) yyerror("Undeclared variable");
        struct node *idnode = mknode(NULL, NULL, (yyvsp[-3].str));
        (yyval.nd) = mknode(idnode, (yyvsp[-1].nd), "assign");
      }
#line 1603 "parser.tab.c"
    break;

  case 12: /* expr: expr '+' term  */
#line 463 "parser.y"
                      { (yyval.nd) = mknode((yyvsp[-2].nd), (yyvsp[0].nd), "+"); }
#line 1609 "parser.tab.c"
    break;

  case 13: /* expr: expr '-' term  */
#line 464 "parser.y"
                      { (yyval.nd) = mknode((yyvsp[-2].nd), (yyvsp[0].nd), "-"); }
#line 1615 "parser.tab.c"
    break;

  case 14: /* expr: term  */
#line 465 "parser.y"
                      { (yyval.nd) = (yyvsp[0].nd); }
#line 1621 "parser.tab.c"
    break;

  case 15: /* term: term '*' factor  */
#line 469 "parser.y"
                      { (yyval.nd) = mknode((yyvsp[-2].nd), (yyvsp[0].nd), "*"); }
#line 1627 "parser.tab.c"
    break;

  case 16: /* term: term '/' factor  */
#line 470 "parser.y"
                      { (yyval.nd) = mknode((yyvsp[-2].nd), (yyvsp[0].nd), "/"); }
#line 1633 "parser.tab.c"
    break;

  case 17: /* term: factor  */
#line 471 "parser.y"
                      { (yyval.nd) = (yyvsp[0].nd); }
#line 1639 "parser.tab.c"
    break;

  case 18: /* factor: '(' expr ')'  */
#line 475 "parser.y"
                      { (yyval.nd) = (yyvsp[-1].nd); }
#line 1645 "parser.tab.c"
    break;

  case 19: /* factor: ID  */
#line 476 "parser.y"
                      { (yyval.nd) = mknode(NULL, NULL, (yyvsp[0].str)); }
#line 1651 "parser.tab.c"
    break;

  case 20: /* factor: NUMBER  */
#line 477 "parser.y"
                      { (yyval.nd) = mkconst((yyvsp[0].num)); }
#line 1657 "parser.tab.c"
    break;

  case 21: /* cond: expr EQ expr  */
#line 481 "parser.y"
                      { (yyval.nd) = mknode((yyvsp[-2].nd), (yyvsp[0].nd), "=="); }
#line 1663 "parser.tab.c"
    break;

  case 22: /* cond: expr NE expr  */
#line 482 "parser.y"
                      { (yyval.nd) = mknode((yyvsp[-2].nd), (yyvsp[0].nd), "!="); }
#line 1669 "parser.tab.c"
    break;

  case 23: /* cond: expr GT expr  */
#line 483 "parser.y"
                      { (yyval.nd) = mknode((yyvsp[-2].nd), (yyvsp[0].nd), ">"); }
#line 1675 "parser.tab.c"
    break;

  case 24: /* cond: expr LT expr  */
#line 484 "parser.y"
                      { (yyval.nd) = mknode((yyvsp[-2].nd), (yyvsp[0].nd), "<"); }
#line 1681 "parser.tab.c"
    break;

  case 25: /* cond: expr GE expr  */
#line 485 "parser.y"
                      { (yyval.nd) = mknode((yyvsp[-2].nd), (yyvsp[0].nd), ">="); }
#line 1687 "parser.tab.c"
    break;

  case 26: /* cond: expr LE expr  */
#line 486 "parser.y"
                      { (yyval.nd) = mknode((yyvsp[-2].nd), (yyvsp[0].nd), "<="); }
#line 1693 "parser.tab.c"
    break;

  case 27: /* if_stmt: IF '(' cond ')' '{' statements '}'  */
#line 490 "parser.y"
                                                            { (yyval.nd) = mknode((yyvsp[-4].nd), mknode((yyvsp[-1].nd), NULL, "else"), "if"); }
#line 1699 "parser.tab.c"
    break;

  case 28: /* if_stmt: IF '(' cond ')' '{' statements '}' ELSE '{' statements '}'  */
#line 491 "parser.y"
                                                                 { (yyval.nd) = mknode((yyvsp[-8].nd), mknode((yyvsp[-5].nd), (yyvsp[-1].nd), "else"), "if"); }
#line 1705 "parser.tab.c"
    break;

  case 29: /* while_stmt: WHILE '(' cond ')' '{' statements '}'  */
#line 495 "parser.y"
                                            { (yyval.nd) = mknode((yyvsp[-4].nd), (yyvsp[-1].nd), "while"); }
#line 1711 "parser.tab.c"
    break;

  case 30: /* print_stmt: PRINT expr ';'  */
#line 499 "parser.y"
                      { (yyval.nd) = mknode((yyvsp[-1].nd), NULL, "print"); }
#line 1717 "parser.tab.c"
    break;


#line 1721 "parser.tab.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 502 "parser.y"
  /* ---------- C epilogue ---------- */

void yyerror(const char *s) {
    fprintf(stderr, "Error: %s at line %d\n", s, yylineno);
    exit(1);
}

int main(void) {
    yyparse();

    printf("\nSymbol Table:\n");
    for (int i = 0; i < HASHSIZE; i++)
        for (struct Symbol *s = hashtable[i]; s; s = s->next)
            printf("%s offset %d\n", s->name, s->offset);

    /* TAC */
    reset_codegen();
    gen_statements(head);
    print_icg();

    /* Simple Assembly */
    print_simple_assembly();

    /* GAS x86-64 */
    label_idx = 0;
    print_gas_assembly();

    /* Run program */
    print_program_output();
    return 0;
}

