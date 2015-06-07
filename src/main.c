#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include <editline/history.h>

#include "mpc.h"

#define LASSERT(args, cond, err) \
  if (!(cond)) { lval_del(args); return lval_err(err); }

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

typedef lval*(*lbuiltin)(lenv*, lval*);


typedef struct lval {
  int type;
  long num;
  char* err;
  char* sym;
  lbuiltin fun;
  int count;
  struct lval** cell;
};

struct lenv {
  int count;
  char** syms;
  lval** vals;
};

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };


//Declarations.
void lval_print(lval* v);
lval* lval_num(long x);
lval* lval_err(char* m);
lval* lval_sym(char* s);
lval* lval_sexpr(void);
void lval_del(lval* v);
lval* lval_add(lval* v, lval* x);
void lval_print(lval* v);
void lval_expr_print(lval* v, char open, char close);
void lval_println(lval* v);
lval* lval_read_num(mpc_ast_t* t);
lval* lval_read(mpc_ast_t* t);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
long exponential(long x, long y);
lval *builtin_op(lval *a, char* op);
lval* lval_eval_sexpr(lval* v);
lval* lval_eval(lval* v);


lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

lval* lval_err(char* m) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_fun(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->fun = func;
  return v;
}

void lval_del(lval* v) {
    switch (v->type) {
        case LVAL_NUM: break;
        case LVAL_FUN: break;

        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;
        }
    free(v);
}


lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

lval* lval_copy(lval* v) {
    lval* x = malloc(sizeof(lval));
    x->type = v->type;
    switch (v->type) {

        case LVAL_FUN: x->fun = v->fun; break;
        case LVAL_NUM: x->num = v->num; break;
    
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err); break;
    
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym); break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
        break;
    }
    return x;
}

void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM:   printf("%li", v->num); break;
    case LVAL_FUN:   printf("<function>"); break;
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
  }
}

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }


lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ?
    lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {

  /* If Symbol or Number return conversion to that type */
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  /* If root (>) or sexpr then create empty list */
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); } 
  if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
  if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }

  /* Fill this list with any valid expression contained within */
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

lval* lval_pop(lval* v, int i) {
    lval* x = v->cell[i];
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));
    v->count--;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

long exponential(long x, long y) {
    if(y == 0) {
        return 1;
    }
    else if(y==1) {
        return x;
    }
    else if(y%2 == 0) {
        return exponential(x*x,y/2);
    }
    else if(y%2 == 1) {
        return x*exponential(x*x, y/2);
    }
}


lval *builtin_op(lval *a, char* op) {
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
        lval_del(a);
        return lval_err("Cannot operate on non-number!");
        }
    }

    lval* x = lval_pop(a, 0);
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    while (a->count > 0) {

        lval* y = lval_pop(a, 0);
        
        if (strcmp(op, "+") == 0 || strcmp(op, "add") == 0) { x->num += y->num; }
        else if (strcmp(op, "-") == 0 || strcmp(op, "sub") == 0) { x->num -= y->num; }
        else if (strcmp(op, "*") == 0 || strcmp(op, "mul") == 0) { x->num *= y->num; }
        else if (strcmp(op, "/") == 0 || strcmp(op, "div") == 0) { 
            if(y->num==0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division By Zero!"); break;
            }
            x->num /= y->num;
        }

        else if (strcmp(op, "%") == 0 || strcmp(op, "mod") == 0) { x->num %= y->num; }
        else if (strcmp(op, "&") == 0 || strcmp(op, "and") == 0) { x->num &= y->num; }
        else if (strcmp(op, "|") == 0 || strcmp(op, "or") == 0) { x->num |= y->num; }
        else if (strcmp(op, "^") == 0 || strcmp(op, "exp") == 0) { 
            x->num += exponential(x->num, y->num); 
        }
        else if (strcmp(op, "min") == 0) {
            if(y->num < x->num) {
                x->num = y->num;
            }
        }
        else if (strcmp(op, "max") == 0) {
            if(y->num > x->num) {
                x->num = y->num;    
            }
        }
        else {
            lval_del(x); lval_del(y);
            x = lval_err("Unknown Symbol!"); break;
        }
        lval_del(y);
    }
    lval_del(a);
    return x;
}

lval* builtin_head(lval* a) {
  LASSERT(a, a->count == 1,
    "Function 'head' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
    "Function 'head' passed incorrect type!");
  LASSERT(a, a->cell[0]->count != 0,
    "Function 'head' passed {}!");

  lval* v = lval_take(a, 0);  
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }
  return v;
}

lval* builtin_tail(lval* a) {
  LASSERT(a, a->count == 1,
    "Function 'tail' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
    "Function 'tail' passed incorrect type!");
  LASSERT(a, a->cell[0]->count != 0,
    "Function 'tail' passed {}!");

  lval* v = lval_take(a, 0);  
  lval_del(lval_pop(v, 0));
  return v;
}

lval* builtin_list(lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_eval(lval* a) {
  LASSERT(a, a->count == 1,
    "Function 'eval' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
    "Function 'eval' passed incorrect type!");

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(x);
}

lval* lval_join(lval* x, lval* y) {
    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }
    lval_del(y);  
    return x;
}

lval* builtin_join(lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
        "Function 'join' passed incorrect type.");
    }
    lval* x = lval_pop(a, 0);
    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }
    lval_del(a);
    return x;
}

lval *builtin_len(lval *a) {
    long count = 0;
    lval* v = lval_take(a, 0);  
    while (v->count > 0) { lval_del(lval_pop(v, 0)); count++; }
    return lval_num(count);
}


lval* builtin(lval* a, char* func) {
  if (strcmp("list", func) == 0) { return builtin_list(a); }
  if (strcmp("head", func) == 0) { return builtin_head(a); }
  if (strcmp("tail", func) == 0) { return builtin_tail(a); }
  if (strcmp("join", func) == 0) { return builtin_join(a); }
  if (strcmp("eval", func) == 0) { return builtin_eval(a); }
  if (strcmp("len", func)  == 0) { return builtin_len(a);  }
  else { return builtin_op(a, func); }
}


lval* lval_eval_sexpr(lval* v) {
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }   

    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }
    if (v->count == 0) { return v; }

    if (v->count == 1) { return lval_take(v, 0); }

    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f); lval_del(v);
        return lval_err("S-expression Does not start with symbol!");
    }

    lval* result = builtin(v, f->sym);
    lval_del(f);
    return result;
}

lval* lval_eval(lval* v) {
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
    return v;
}


void print_init_message() {
    puts("Vhisp 0.5");
    puts("Press Ctrl+c to exit.\n");
}


int main() {

    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Vhisp = mpc_new("vhisp");

    mpca_lang(MPCA_LANG_DEFAULT,
      "                                                                 \
        number   : /-?[0-9]+(\\.[0-9]+)?/ ;                             \
        symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/                     \
        sexpr    : '(' <expr>* ')' ;                                    \
        qexpr    : '{' <expr>* '}' ;                                    \
        expr     : <number> | <symbol> | <sexpr> | <qexpr> ;            \
        vhisp    : /^/ <expr>* /$/ ;                                    \
    ",
    Number, Symbol, Expr, Sexpr, Qexpr, Vhisp);

    print_init_message();
    
    while(1) {
        
        char* input  = readline("vhisp% ");
        
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Vhisp, &r)) {
            lval* x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } 
        else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        add_history(input);
        free(input);
    }
    puts("vhisp exited");
    mpc_cleanup(5, Number, Symbol, Expr, Sexpr, Qexpr, Vhisp);
    return 0;
}
