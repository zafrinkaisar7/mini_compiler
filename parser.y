%{
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
%}

/* ---------- Bison declarations ---------- */
%union {
    char str[100];
    int num;
    struct node *nd;
}
%token <str> ID
%token <num> NUMBER
%token INT IF ELSE WHILE PRINT EQ NE GT LT GE LE
%type <nd> program statements statement decl assign expr term factor cond if_stmt while_stmt print_stmt
%left '+' '-'
%left '*' '/'

%%  /* ---------- Grammar rules ---------- */

program : statements { head = $1; $$ = $1; } ;

statements
    : statement statements { $$ = mknode($1, $2, "statements"); }
    | /* empty */          { $$ = NULL; }
    ;

statement
    : decl         { $$ = $1; }
    | assign       { $$ = $1; }
    | if_stmt      { $$ = $1; }
    | while_stmt   { $$ = $1; }
    | print_stmt   { $$ = $1; }
    ;

decl
    : INT ID ';'
      {
        if (lookup($2)) yyerror("Redeclaration");
        insert($2, -8 * (++var_count));  /* negative offsets: safe */
        struct node *idnode = mknode(NULL, NULL, $2);
        $$ = mknode(idnode, NULL, "decl");
      }
    ;

assign
    : ID '=' expr ';'
      {
        if (!lookup($1)) yyerror("Undeclared variable");
        struct node *idnode = mknode(NULL, NULL, $1);
        $$ = mknode(idnode, $3, "assign");
      }
    ;

expr
    : expr '+' term   { $$ = mknode($1, $3, "+"); }
    | expr '-' term   { $$ = mknode($1, $3, "-"); }
    | term            { $$ = $1; }
    ;

term
    : term '*' factor { $$ = mknode($1, $3, "*"); }
    | term '/' factor { $$ = mknode($1, $3, "/"); }
    | factor          { $$ = $1; }
    ;

factor
    : '(' expr ')'    { $$ = $2; }
    | ID              { $$ = mknode(NULL, NULL, $1); }
    | NUMBER          { $$ = mkconst($1); }
    ;

cond
    : expr EQ expr    { $$ = mknode($1, $3, "=="); }
    | expr NE expr    { $$ = mknode($1, $3, "!="); }
    | expr GT expr    { $$ = mknode($1, $3, ">"); }
    | expr LT expr    { $$ = mknode($1, $3, "<"); }
    | expr GE expr    { $$ = mknode($1, $3, ">="); }
    | expr LE expr    { $$ = mknode($1, $3, "<="); }
    ;

if_stmt
    : IF '(' cond ')' '{' statements '}'                    { $$ = mknode($3, mknode($6, NULL, "else"), "if"); }
    | IF '(' cond ')' '{' statements '}' ELSE '{' statements '}' { $$ = mknode($3, mknode($6, $10, "else"), "if"); }
    ;

while_stmt
    : WHILE '(' cond ')' '{' statements '}' { $$ = mknode($3, $6, "while"); }
    ;

print_stmt
    : PRINT expr ';'  { $$ = mknode($2, NULL, "print"); }
    ;

%%  /* ---------- C epilogue ---------- */

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

