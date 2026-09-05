/*
 * cc.c - a tiny two-pass C compiler for koppi's OS.
 *
 * Pass 1  (front end): lex -> parse -> semantic analysis, producing a typed
 *                      AST and an insertion-ordered symbol table.
 * Pass 2  (back end):  walk the AST emitting i386 machine code into a byte
 *                      buffer with a relocation list, then lay out one
 *                      PT_LOAD ELF image at 0x800000 and patch the relocations.
 *
 * There is no assembler or linker step: the output of a single run is a
 * ready-to-load ELF executable for this OS's loader (elf.c).
 *
 * The compiler is written in the same C subset it accepts, so it can compile
 * itself.  That means: no #include, no preprocessor, no float/double, no
 * "long long" (long is 32-bit), no pass-by-value structs, and the standard
 * library surface it needs is provided by the auto-prepended prelude.c
 * (functions xalloc/xrealloc/sys_readfile/sys_writefile/sys_out/sys_exit).
 *
 * See NOTES.md for the full accepted subset and the ABI.
 */

/* ------------------------------------------------------------------ *
 *  externals provided by the backend (sys_native.c / prelude.c)
 * ------------------------------------------------------------------ */
void *xalloc(int n);              /* zeroed; never freed (arena style) */
void *xrealloc(void *p, int n);
char *sys_readfile(char *path, int *plen);   /* whole file; 0 on failure */
int   sys_writefile(char *path, void *buf, int len);
void  sys_out(char *buf, int len);           /* raw bytes to stdout */
void  sys_exit(int code);

/* ------------------------------------------------------------------ *
 *  small runtime helpers (self-contained, unique names)
 * ------------------------------------------------------------------ */
int xstrlen(char *s) { int n = 0; while (s[n]) n++; return n; }

int xstreq(char *a, char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

int xstrneq(char *a, char *b, int n) {
    int i = 0;
    while (i < n) {
        if (a[i] != b[i]) return 0;
        if (a[i] == 0) return 1;
        i++;
    }
    return 1;
}

void xmemcpy(char *d, char *s, int n) { int i = 0; while (i < n) { d[i] = s[i]; i++; } }
void xmemset(char *d, int c, int n) { int i = 0; while (i < n) { d[i] = (char)c; i++; } }

char *xstrndup(char *s, int n) {
    char *p = xalloc(n + 1);
    xmemcpy(p, s, n);
    p[n] = 0;
    return p;
}
char *xstrdup(char *s) { return xstrndup(s, xstrlen(s)); }

int xatoi(char *s) {
    int n = 0, neg = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
    return neg ? -n : n;
}

/* non-varargs output helpers (cc.c avoids stdarg so it stays self-hostable) */
void outs(char *s) { sys_out(s, xstrlen(s)); }
void outd(int v) {
    char b[16]; int n = 0;
    unsigned u = (unsigned)v;
    if (v < 0) { sys_out("-", 1); u = (unsigned)(-v); }
    if (u == 0) { sys_out("0", 1); return; }
    char t[16]; int k = 0;
    while (u) { t[k++] = '0' + (u % 10); u /= 10; }
    while (k) b[n++] = t[--k];
    sys_out(b, n);
}
void outx(unsigned u) {
    char t[16]; int k = 0;
    if (u == 0) { sys_out("0", 1); return; }
    while (u) { int d = u & 15; t[k++] = d < 10 ? '0' + d : 'a' + d - 10; u >>= 4; }
    char b[16]; int n = 0;
    while (k) b[n++] = t[--k];
    sys_out(b, n);
}

/* ------------------------------------------------------------------ *
 *  globals: source, diagnostics
 * ------------------------------------------------------------------ */
char *g_src;                 /* whole concatenated source (prelude + user) */
char *g_user_name = "input";
int   g_prelude_lines = 0;   /* lines contributed by the prelude */

void loc_of(int line, char **pfile, int *pline) {
    if (g_prelude_lines && line > g_prelude_lines) {
        *pfile = g_user_name;
        *pline = line - g_prelude_lines;
    } else {
        *pfile = g_prelude_lines ? "prelude.c" : g_user_name;
        *pline = line;
    }
}

void die_at(int line, char *msg, char *extra) {
    char *f; int l;
    loc_of(line, &f, &l);
    outs(f); outs(":"); outd(l); outs(": error: "); outs(msg);
    if (extra) outs(extra);
    outs("\n");
    sys_exit(1);
}

void die(char *msg) {
    outs("cc: error: "); outs(msg); outs("\n");
    sys_exit(1);
}

/* ------------------------------------------------------------------ *
 *  lexer
 * ------------------------------------------------------------------ */
enum {
    T_EOF, T_NUM, T_STR, T_CHAR, T_IDENT, T_PUNCT, T_KW
};

typedef struct Token Token;
struct Token {
    int kind;
    int line;
    long val;           /* T_NUM / T_CHAR value */
    char *str;          /* identifier / keyword / punctuator text (NUL-term) */
    char *sval;         /* T_STR bytes (may contain NUL) */
    int slen;           /* T_STR length */
    Token *next;
};

char *g_kw[] = {
    "int", "char", "void", "short", "long", "unsigned", "signed", "_Bool",
    "struct", "union", "enum", "typedef", "static", "extern", "const",
    "volatile", "register", "inline", "sizeof", "return", "if", "else",
    "while", "for", "do", "break", "continue", "switch", "case", "default",
    "goto", "_Noreturn", "restrict", 0
};

int is_kw(char *s, int n) {
    int i = 0;
    while (g_kw[i]) {
        if (xstrneq(g_kw[i], s, n) && (int)xstrlen(g_kw[i]) == n) return 1;
        i++;
    }
    return 0;
}

int is_alpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
int is_digit(int c) { return c >= '0' && c <= '9'; }
int is_alnum(int c) { return is_alpha(c) || is_digit(c); }
int is_space(int c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; }

int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

Token *new_tok(int kind, int line) {
    Token *t = xalloc(sizeof(Token));
    t->kind = kind;
    t->line = line;
    return t;
}

/* read one char escape starting at *pp (which points past the backslash) */
int read_escape(char **pp) {
    char *p = *pp;
    int c = *p++;
    int v;
    switch (c) {
    case 'n': v = '\n'; break;
    case 't': v = '\t'; break;
    case 'r': v = '\r'; break;
    case '0': v = 0; break;
    case '\\': v = '\\'; break;
    case '\'': v = '\''; break;
    case '"': v = '"'; break;
    case 'a': v = 7; break;
    case 'b': v = 8; break;
    case 'f': v = 12; break;
    case 'v': v = 11; break;
    case 'e': v = 27; break;
    case 'x': {
        v = 0;
        while (hexval(*p) >= 0) { v = v * 16 + hexval(*p); p++; }
        break;
    }
    default:
        if (c >= '1' && c <= '7') {
            v = c - '0';
            while (*p >= '0' && *p <= '7') { v = v * 8 + (*p - '0'); p++; }
        } else {
            v = c;
        }
    }
    *pp = p;
    return v;
}

char *g_puncts[] = {
    "...", "<<=", ">>=",
    "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||",
    "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=",
    "+", "-", "*", "/", "%", "&", "|", "^", "~", "!", "=", "<", ">",
    "(", ")", "[", "]", "{", "}", ";", ",", ".", "?", ":",
    0
};

Token *lex(char *src) {
    Token head;
    head.next = 0;
    Token *cur = &head;
    char *p = src;
    int line = 1;

    while (*p) {
        if (*p == '\n') { line++; p++; continue; }
        if (is_space(*p)) { p++; continue; }

        /* line comment */
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }
        /* block comment */
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) {
                if (*p == '\n') line++;
                p++;
            }
            if (*p) p += 2;
            continue;
        }
        /* preprocessor line: skip to end of line (no preprocessor) */
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        /* number: decimal / hex / octal, optional u/l/U/L suffix */
        if (is_digit(*p) || (p[0] == '.' && is_digit(p[1]))) {
            long v = 0;
            if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
                p += 2;
                while (hexval(*p) >= 0) { v = v * 16 + hexval(*p); p++; }
            } else if (p[0] == '0' && is_digit(p[1])) {
                p++;
                while (*p >= '0' && *p <= '7') { v = v * 8 + (*p - '0'); p++; }
            } else {
                while (is_digit(*p)) { v = v * 10 + (*p - '0'); p++; }
            }
            while (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L') p++;
            Token *t = new_tok(T_NUM, line);
            t->val = v;
            cur->next = t; cur = t;
            continue;
        }

        /* char literal */
        if (*p == '\'') {
            p++;
            int v;
            if (*p == '\\') { p++; v = read_escape(&p); }
            else v = (unsigned char)*p++;
            if (*p == '\'') p++;
            Token *t = new_tok(T_CHAR, line);
            t->val = v;
            cur->next = t; cur = t;
            continue;
        }

        /* string literal (with adjacent concatenation) */
        if (*p == '"') {
            char *buf = xalloc(1);
            int cap = 1, n = 0;
            for (;;) {
                p++;                       /* skip opening quote */
                while (*p && *p != '"') {
                    int c;
                    if (*p == '\\') { p++; c = read_escape(&p); }
                    else c = (unsigned char)*p++;
                    if (n + 1 >= cap) { cap *= 2; if (cap < 16) cap = 16; buf = xrealloc(buf, cap); }
                    buf[n++] = (char)c;
                }
                if (*p == '"') p++;
                /* skip whitespace/comments, check for another string */
                char *q = p;
                while (is_space(*q) || (q[0] == '/' && q[1] == '/') || (q[0] == '/' && q[1] == '*')) {
                    if (q[0] == '/' && q[1] == '/') { while (*q && *q != '\n') q++; }
                    else if (q[0] == '/' && q[1] == '*') { q += 2; while (*q && !(q[0] == '*' && q[1] == '/')) q++; if (*q) q += 2; }
                    else { if (*q == '\n') line++; q++; }
                }
                if (*q == '"') { p = q; continue; }
                break;
            }
            if (n + 1 >= cap) buf = xrealloc(buf, n + 1);
            buf[n] = 0;
            Token *t = new_tok(T_STR, line);
            t->sval = buf;
            t->slen = n;
            cur->next = t; cur = t;
            continue;
        }

        /* identifier / keyword */
        if (is_alpha(*p)) {
            char *s = p;
            while (is_alnum(*p)) p++;
            int n = p - s;
            Token *t = new_tok(is_kw(s, n) ? T_KW : T_IDENT, line);
            t->str = xstrndup(s, n);
            cur->next = t; cur = t;
            continue;
        }

        /* punctuator (longest match) */
        {
            int i = 0, matched = 0;
            while (g_puncts[i]) {
                int L = xstrlen(g_puncts[i]);
                if (xstrneq(g_puncts[i], p, L) && (int)xstrlen(g_puncts[i]) == L) {
                    int ok = 1, k = 0;
                    while (k < L) { if (p[k] != g_puncts[i][k]) { ok = 0; break; } k++; }
                    if (ok) {
                        Token *t = new_tok(T_PUNCT, line);
                        t->str = g_puncts[i];
                        cur->next = t; cur = t;
                        p += L;
                        matched = 1;
                        break;
                    }
                }
                i++;
            }
            if (!matched) {
                char m[2]; m[0] = *p; m[1] = 0;
                die_at(line, "stray character in program: ", m);
            }
        }
    }
    Token *t = new_tok(T_EOF, line);
    cur->next = t;
    return head.next;
}

/* ------------------------------------------------------------------ *
 *  token stream cursor
 * ------------------------------------------------------------------ */
Token *TK;                   /* current token */

int tk_is(char *s) {
    return (TK->kind == T_PUNCT || TK->kind == T_KW) && xstreq(TK->str, s);
}
int tk_is_kw(char *s) { return TK->kind == T_KW && xstreq(TK->str, s); }

void tk_next(void) { TK = TK->next; }

int tk_eat(char *s) {
    if (tk_is(s)) { tk_next(); return 1; }
    return 0;
}

void tk_expect(char *s) {
    if (!tk_is(s)) {
        die_at(TK->line, "expected '", s);   /* extra printed without closing quote; ok */
    }
    tk_next();
}

char *tk_ident(void) {
    if (TK->kind != T_IDENT) die_at(TK->line, "expected an identifier", 0);
    char *s = TK->str;
    tk_next();
    return s;
}

/* ------------------------------------------------------------------ *
 *  types
 * ------------------------------------------------------------------ */
enum {
    TY_VOID, TY_BOOL, TY_CHAR, TY_SHORT, TY_INT, TY_PTR, TY_ARRAY,
    TY_STRUCT, TY_UNION, TY_FUNC
};

typedef struct Type Type;
typedef struct Member Member;
typedef struct Obj Obj;
typedef struct Node Node;

struct Member {
    char *name;
    Type *ty;
    int offset;
    Member *next;
};

struct Type {
    int kind;
    int size;
    int align;
    int is_unsigned;
    Type *base;          /* pointer/array element, or function return */
    int array_len;
    Member *members;     /* struct/union */
    char *tag;           /* struct/union/enum tag, or 0 */
    Type *params;        /* function: linked list via ->next using base? */
    Type *next;          /* used to chain function params */
    int is_variadic;
    int is_incomplete;
};

Type *ty_void; Type *ty_bool; Type *ty_char; Type *ty_short;
Type *ty_int; Type *ty_uint; Type *ty_uchar; Type *ty_ushort;

Type *new_type(int kind, int size, int align) {
    Type *t = xalloc(sizeof(Type));
    t->kind = kind;
    t->size = size;
    t->align = align;
    return t;
}

void init_types(void) {
    ty_void = new_type(TY_VOID, 1, 1);
    ty_bool = new_type(TY_BOOL, 1, 1);
    ty_char = new_type(TY_CHAR, 1, 1);
    ty_short = new_type(TY_SHORT, 2, 2);
    ty_int = new_type(TY_INT, 4, 4);
    ty_uint = new_type(TY_INT, 4, 4); ty_uint->is_unsigned = 1;
    ty_uchar = new_type(TY_CHAR, 1, 1); ty_uchar->is_unsigned = 1;
    ty_ushort = new_type(TY_SHORT, 2, 2); ty_ushort->is_unsigned = 1;
}

Type *pointer_to(Type *base) {
    Type *t = new_type(TY_PTR, 4, 4);
    t->base = base;
    t->is_unsigned = 1;
    return t;
}

Type *array_of(Type *base, int len) {
    Type *t = new_type(TY_ARRAY, base->size * len, base->align);
    t->base = base;
    t->array_len = len;
    return t;
}

Type *func_type(Type *ret) {
    Type *t = new_type(TY_FUNC, 1, 1);
    t->base = ret;
    return t;
}

int is_integer(Type *t) {
    int k = t->kind;
    return k == TY_BOOL || k == TY_CHAR || k == TY_SHORT || k == TY_INT;
}
int is_pointer(Type *t) { return t->kind == TY_PTR || t->kind == TY_ARRAY; }
int is_scalar(Type *t) { return is_integer(t) || t->kind == TY_PTR; }

Type *decay(Type *t) {
    if (t->kind == TY_ARRAY) return pointer_to(t->base);
    if (t->kind == TY_FUNC) return pointer_to(t);
    return t;
}

int align_to(int n, int a) { return (n + a - 1) / a * a; }

/* ------------------------------------------------------------------ *
 *  symbols and scopes
 * ------------------------------------------------------------------ */
enum { OBJ_VAR, OBJ_FUNC, OBJ_ENUM };

typedef struct Reloc Reloc;
struct Reloc {
    int offset;         /* byte offset within this global's init image */
    Obj *target;        /* target symbol (global/func) */
    char *strtarget;    /* or a string-literal pointer */
    int strlen_;
    int addend;
    Reloc *next;
};

struct Obj {
    char *name;
    Type *ty;
    int kind;
    int is_local;
    int is_static;
    int is_definition;
    int is_extern;
    /* locals */
    int offset;         /* ebp-relative (negative) */
    /* globals */
    int data_off;       /* offset within data section */
    char *init_data;
    int init_len;
    Reloc *relocs;
    /* functions */
    Obj *params;
    Node *body;
    int stack_size;
    int code_off;       /* offset within code section, filled in pass 2 */
    int is_variadic;
    /* enum constant */
    long enum_val;

    Obj *next;          /* global list / param list */
};

typedef struct Scope Scope;
typedef struct ScopeVar ScopeVar;
struct ScopeVar {
    char *name;
    Obj *var;
    Type *type_def;     /* typedef */
    Type *tag;          /* struct/union/enum tag */
    ScopeVar *next;
};
struct Scope {
    ScopeVar *vars;
    Scope *next;
};

Scope *g_scope;
Obj *g_globals;             /* insertion-ordered list, head */
Obj *g_globals_tail;
Obj *g_cur_fn;

/* string pool */
typedef struct Str Str;
struct Str {
    char *data;
    int len;
    int data_off;
    Str *next;
};
Str *g_strs;
Str *g_strs_tail;

void enter_scope(void) {
    Scope *s = xalloc(sizeof(Scope));
    s->next = g_scope;
    g_scope = s;
}
void leave_scope(void) { g_scope = g_scope->next; }

ScopeVar *push_scopevar(char *name) {
    ScopeVar *sv = xalloc(sizeof(ScopeVar));
    sv->name = name;
    sv->next = g_scope->vars;
    g_scope->vars = sv;
    return sv;
}

Obj *find_var(char *name) {
    Scope *s = g_scope;
    while (s) {
        ScopeVar *sv = s->vars;
        while (sv) {
            if (sv->var && xstreq(sv->name, name)) return sv->var;
            sv = sv->next;
        }
        s = s->next;
    }
    return 0;
}

Type *find_typedef(char *name) {
    Scope *s = g_scope;
    while (s) {
        ScopeVar *sv = s->vars;
        while (sv) {
            if (sv->type_def && xstreq(sv->name, name)) return sv->type_def;
            sv = sv->next;
        }
        s = s->next;
    }
    return 0;
}

Type *find_tag(char *name) {
    Scope *s = g_scope;
    while (s) {
        ScopeVar *sv = s->vars;
        while (sv) {
            if (sv->tag && sv->name && xstreq(sv->name, name)) return sv->tag;
            sv = sv->next;
        }
        s = s->next;
    }
    return 0;
}
Type *find_tag_cur(char *name) {
    ScopeVar *sv = g_scope->vars;
    while (sv) {
        if (sv->tag && sv->name && xstreq(sv->name, name)) return sv->tag;
        sv = sv->next;
    }
    return 0;
}

Obj *new_global(char *name, Type *ty, int kind) {
    Obj *o = xalloc(sizeof(Obj));
    o->name = name;
    o->ty = ty;
    o->kind = kind;
    if (!g_globals) g_globals = o; else g_globals_tail->next = o;
    g_globals_tail = o;
    if (name && g_scope) push_scopevar(name)->var = o;
    return o;
}

Obj *new_local(char *name, Type *ty) {
    Obj *o = xalloc(sizeof(Obj));
    o->name = name;
    o->ty = ty;
    o->kind = OBJ_VAR;
    o->is_local = 1;
    if (name) push_scopevar(name)->var = o;
    return o;
}

int add_string(char *data, int len, char **pdata_hint) {
    (void)pdata_hint;
    Str *s = xalloc(sizeof(Str));
    s->data = data;
    s->len = len;
    if (!g_strs) g_strs = s; else g_strs_tail->next = s;
    g_strs_tail = s;
    return 0;
}

/* ------------------------------------------------------------------ *
 *  AST
 * ------------------------------------------------------------------ */
enum {
    ND_NUM, ND_VAR, ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_MOD, ND_NEG,
    ND_EQ, ND_NE, ND_LT, ND_LE, ND_ASSIGN, ND_ADDR, ND_DEREF, ND_MEMBER,
    ND_CALL, ND_COMMA, ND_COND, ND_LOGAND, ND_LOGOR, ND_NOT, ND_BITNOT,
    ND_BITAND, ND_BITOR, ND_BITXOR, ND_SHL, ND_SHR, ND_CAST,
    ND_RETURN, ND_IF, ND_FOR, ND_DO, ND_BLOCK, ND_EXPR_STMT, ND_NULL,
    ND_BREAK, ND_CONTINUE, ND_SWITCH, ND_CASE, ND_LABEL, ND_GOTO,
    ND_STMT_EXPR
};

typedef struct CaseLabel CaseLabel;
struct CaseLabel {
    long val;
    int is_default;
    int label;
    Node *body;
    CaseLabel *next;
};

struct Node {
    int kind;
    Type *ty;
    int line;
    Node *lhs, *rhs;
    Node *cond, *then, *els, *init, *inc;
    Node *body;          /* block body list / stmt-expr list */
    Node *next;          /* list link */
    Obj *var;            /* ND_VAR */
    long val;            /* ND_NUM */
    Str *str;            /* ND_STR handled as ND_VAR-like -> use var==0,str set */
    char *member_name;
    Member *member;
    Node *args;          /* ND_CALL argument list (via ->next) */
    Node *fn;            /* ND_CALL: callee expression */
    CaseLabel *cases;    /* ND_SWITCH */
    char *label_name;    /* ND_GOTO / ND_LABEL */
    int brk_label, cont_label;   /* loop/switch */
};

Node *new_node(int kind, int line) {
    Node *n = xalloc(sizeof(Node));
    n->kind = kind;
    n->line = line;
    return n;
}
Node *new_binary(int kind, Node *lhs, Node *rhs, int line) {
    Node *n = new_node(kind, line);
    n->lhs = lhs; n->rhs = rhs;
    return n;
}
Node *new_unary(int kind, Node *lhs, int line) {
    Node *n = new_node(kind, line);
    n->lhs = lhs;
    return n;
}
Node *new_num(long v, int line) {
    Node *n = new_node(ND_NUM, line);
    n->val = v;
    n->ty = ty_int;
    return n;
}
Node *new_var_node(Obj *var, int line) {
    Node *n = new_node(ND_VAR, line);
    n->var = var;
    n->ty = var->ty;
    return n;
}

/* forward decls */
Type *declspec(int *storage);
Type *declarator(Type *ty, char **name);
Node *expr(void);
Node *assign(void);
Node *conditional(void);
Node *compound_stmt(void);
Node *stmt(void);
void gen_program(void);
long const_expr(void);
Node *cast_expr(void);

/* ------------------------------------------------------------------ *
 *  type utilities on nodes
 * ------------------------------------------------------------------ */
Type *common_type(Type *a, Type *b) {
    if (a->kind == TY_PTR || a->kind == TY_ARRAY) return decay(a);
    if (b->kind == TY_PTR || b->kind == TY_ARRAY) return decay(b);
    if ((a->size == 4 && a->is_unsigned) || (b->size == 4 && b->is_unsigned))
        return ty_uint;
    return ty_int;
}

void add_type(Node *n);

Node *new_cast(Node *e, Type *ty) {
    add_type(e);
    Node *n = new_node(ND_CAST, e->line);
    n->lhs = e;
    n->ty = ty;
    return n;
}

void usual_arith(Node *n) {
    Type *t = common_type(n->lhs->ty, n->rhs->ty);
    n->lhs = new_cast(n->lhs, t);
    n->rhs = new_cast(n->rhs, t);
    n->ty = t;
}

Node *ptr_add(Node *lhs, Node *rhs, int line) {
    add_type(lhs); add_type(rhs);
    if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
        Node *n = new_binary(ND_ADD, lhs, rhs, line);
        usual_arith(n);
        return n;
    }
    if (is_pointer(rhs->ty) && is_integer(lhs->ty)) {
        Node *t = lhs; lhs = rhs; rhs = t;
    }
    if (is_pointer(lhs->ty) && is_integer(rhs->ty)) {
        int sz = lhs->ty->base->size;
        if (sz == 0) sz = 1;
        rhs = new_binary(ND_MUL, rhs, new_num(sz, line), line);
        rhs->ty = ty_int;
        Node *n = new_binary(ND_ADD, lhs, rhs, line);
        n->ty = decay(lhs->ty);
        return n;
    }
    die_at(line, "invalid operands to +", 0);
    return 0;
}

Node *ptr_sub(Node *lhs, Node *rhs, int line) {
    add_type(lhs); add_type(rhs);
    if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
        Node *n = new_binary(ND_SUB, lhs, rhs, line);
        usual_arith(n);
        return n;
    }
    if (is_pointer(lhs->ty) && is_integer(rhs->ty)) {
        int sz = lhs->ty->base->size;
        if (sz == 0) sz = 1;
        rhs = new_binary(ND_MUL, rhs, new_num(sz, line), line);
        rhs->ty = ty_int;
        Node *n = new_binary(ND_SUB, lhs, rhs, line);
        n->ty = decay(lhs->ty);
        return n;
    }
    if (is_pointer(lhs->ty) && is_pointer(rhs->ty)) {
        int sz = lhs->ty->base->size;
        if (sz == 0) sz = 1;
        Node *n = new_binary(ND_SUB, lhs, rhs, line);
        n->ty = ty_int;
        Node *d = new_binary(ND_DIV, n, new_num(sz, line), line);
        d->ty = ty_int;
        return d;
    }
    die_at(line, "invalid operands to -", 0);
    return 0;
}

Member *find_member(Type *ty, char *name) {
    Member *m = ty->members;
    while (m) {
        if (xstreq(m->name, name)) return m;
        m = m->next;
    }
    return 0;
}

void add_type(Node *n) {
    if (!n || n->ty) return;

    add_type(n->lhs);
    add_type(n->rhs);
    add_type(n->cond);
    add_type(n->then);
    add_type(n->els);
    add_type(n->init);
    add_type(n->inc);
    add_type(n->fn);
    for (Node *c = n->body; c; c = c->next) add_type(c);
    for (Node *a = n->args; a; a = a->next) add_type(a);

    switch (n->kind) {
    case ND_NUM: n->ty = ty_int; return;
    case ND_ADD: case ND_SUB: case ND_MUL: case ND_DIV: case ND_MOD:
    case ND_BITAND: case ND_BITOR: case ND_BITXOR:
        usual_arith(n);
        return;
    case ND_SHL: case ND_SHR:
        n->ty = n->lhs->ty->is_unsigned ? ty_uint : ty_int;
        if (n->lhs->ty->size < 4) n->ty = ty_int;
        return;
    case ND_NEG: n->ty = n->lhs->ty; return;
    case ND_BITNOT: n->ty = n->lhs->ty; return;
    case ND_ASSIGN:
        n->ty = n->lhs->ty;
        if (n->ty->kind != TY_STRUCT && n->ty->kind != TY_UNION)
            n->rhs = new_cast(n->rhs, n->ty);
        return;
    case ND_EQ: case ND_NE: case ND_LT: case ND_LE:
    case ND_LOGAND: case ND_LOGOR: case ND_NOT:
        n->ty = ty_int;
        return;
    case ND_COMMA: n->ty = n->rhs->ty; return;
    case ND_COND:
        if (n->then->ty->kind == TY_VOID || n->els->ty->kind == TY_VOID)
            n->ty = ty_void;
        else if (is_pointer(n->then->ty))
            n->ty = decay(n->then->ty);
        else if (is_pointer(n->els->ty))
            n->ty = decay(n->els->ty);
        else
            n->ty = common_type(n->then->ty, n->els->ty);
        return;
    case ND_MEMBER: {
        Type *st = n->lhs->ty;
        if (st->kind != TY_STRUCT && st->kind != TY_UNION)
            die_at(n->line, "member access on non-struct", 0);
        Member *m = find_member(st, n->member_name);
        if (!m) die_at(n->line, "no such member: ", n->member_name);
        n->member = m;
        n->ty = m->ty;
        return;
    }
    case ND_ADDR:
        if (n->lhs->ty->kind == TY_ARRAY)
            n->ty = pointer_to(n->lhs->ty->base);
        else
            n->ty = pointer_to(n->lhs->ty);
        return;
    case ND_DEREF:
        if (!is_pointer(n->lhs->ty))
            die_at(n->line, "dereference of non-pointer", 0);
        n->ty = n->lhs->ty->base;
        if (!n->ty) n->ty = ty_char;
        if (n->ty->kind == TY_VOID)
            die_at(n->line, "dereference of void pointer", 0);
        return;
    case ND_CALL:
        return;        /* ty set by parser */
    case ND_STMT_EXPR: {
        Node *last = 0;
        for (Node *c = n->body; c; c = c->next) last = c;
        if (last && last->kind == ND_EXPR_STMT) n->ty = last->lhs->ty;
        else n->ty = ty_void;
        return;
    }
    case ND_VAR:
        return;
    case ND_CAST:
        return;
    default:
        n->ty = ty_int;
        return;
    }
}

/* ================================================================== *
 *  PARSER  (pass 1)
 * ================================================================== */

int g_next_label = 1;
int new_label(void) { return g_next_label++; }

int is_typename(void) {
    if (TK->kind == T_KW) {
        return tk_is_kw("int") || tk_is_kw("char") || tk_is_kw("void") ||
               tk_is_kw("short") || tk_is_kw("long") || tk_is_kw("unsigned") ||
               tk_is_kw("signed") || tk_is_kw("_Bool") || tk_is_kw("struct") ||
               tk_is_kw("union") || tk_is_kw("enum") || tk_is_kw("const") ||
               tk_is_kw("volatile") || tk_is_kw("static") || tk_is_kw("extern") ||
               tk_is_kw("typedef") || tk_is_kw("register") || tk_is_kw("inline") ||
               tk_is_kw("_Noreturn") || tk_is_kw("restrict");
    }
    if (TK->kind == T_IDENT) return find_typedef(TK->str) != 0;
    return 0;
}

Type *struct_union_type(int kind);
Type *enum_type(void);

/* declspec: parse leading type/storage keywords, return the base type.
 * *storage: bit 1 = typedef, 2 = static, 4 = extern  */
enum { SC_TYPEDEF = 1, SC_STATIC = 2, SC_EXTERN = 4 };

Type *declspec(int *storage) {
    enum { VOID = 1, BOOL = 2, CHAR = 4, SHORT = 8, INT = 16, LONG = 32,
           SIGNED = 64, UNSIGNED = 128 };
    int counter = 0;
    Type *ty = ty_int;
    int found_user_type = 0;
    if (storage) *storage = 0;

    for (;;) {
        if (tk_is_kw("typedef")) { if (storage) *storage |= SC_TYPEDEF; tk_next(); continue; }
        if (tk_is_kw("static"))  { if (storage) *storage |= SC_STATIC;  tk_next(); continue; }
        if (tk_is_kw("extern"))  { if (storage) *storage |= SC_EXTERN;  tk_next(); continue; }
        if (tk_is_kw("const") || tk_is_kw("volatile") || tk_is_kw("register") ||
            tk_is_kw("inline") || tk_is_kw("_Noreturn") || tk_is_kw("restrict")) {
            tk_next(); continue;
        }

        if (tk_is_kw("struct")) { tk_next(); ty = struct_union_type(TY_STRUCT); found_user_type = 1; break; }
        if (tk_is_kw("union"))  { tk_next(); ty = struct_union_type(TY_UNION);  found_user_type = 1; break; }
        if (tk_is_kw("enum"))   { tk_next(); ty = enum_type();                  found_user_type = 1; break; }

        if (TK->kind == T_IDENT) {
            Type *td = find_typedef(TK->str);
            if (td && counter == 0) { ty = td; tk_next(); found_user_type = 1; break; }
            if (!td) break;
            if (td) break;
        }

        if (tk_is_kw("void"))     { counter += VOID;   tk_next(); continue; }
        if (tk_is_kw("_Bool"))    { counter += BOOL;   tk_next(); continue; }
        if (tk_is_kw("char"))     { counter += CHAR;   tk_next(); continue; }
        if (tk_is_kw("short"))    { counter += SHORT;  tk_next(); continue; }
        if (tk_is_kw("int"))      { counter += INT;    tk_next(); continue; }
        if (tk_is_kw("long"))     { counter += LONG;   tk_next(); continue; }
        if (tk_is_kw("signed"))   { counter |= SIGNED; tk_next(); continue; }
        if (tk_is_kw("unsigned")) { counter |= UNSIGNED; tk_next(); continue; }
        break;
    }

    if (!found_user_type) {
        int u = (counter & UNSIGNED) != 0;
        int base = counter & ~(SIGNED | UNSIGNED);
        if (base == VOID) ty = ty_void;
        else if (base == BOOL) ty = ty_bool;
        else if (base == CHAR) ty = u ? ty_uchar : ty_char;
        else if (base == SHORT || base == (SHORT + INT)) ty = u ? ty_ushort : ty_short;
        else ty = u ? ty_uint : ty_int;   /* int, long, long long -> 32-bit */
    }
    return ty;
}

/* parse function parameter list after '(' has been consumed */
Type *func_params(Type *ret) {
    Type *fty = func_type(ret);
    Type head; head.next = 0;
    Type *cur = &head;

    if (tk_is("void") && TK->next && TK->next->kind == T_PUNCT && xstreq(TK->next->str, ")")) {
        tk_next();          /* consume 'void' */
        tk_next();          /* consume ')' */
        fty->params = 0;
        return fty;
    }
    if (tk_eat(")")) { fty->params = 0; return fty; }

    for (;;) {
        if (tk_eat("...")) { fty->is_variadic = 1; break; }
        int sc;
        Type *bt = declspec(&sc);
        char *nm = 0;
        Type *pt = declarator(bt, &nm);
        if (pt->kind == TY_ARRAY) pt = pointer_to(pt->base);
        if (pt->kind == TY_FUNC) pt = pointer_to(pt);
        Type *copy = xalloc(sizeof(Type));
        *copy = *pt;
        copy->next = 0;
        copy->tag = nm;      /* stash param name in ->tag for funcdef use */
        cur->next = copy;
        cur = copy;
        if (!tk_eat(",")) break;
    }
    tk_expect(")");
    fty->params = head.next;
    return fty;
}

/* type_suffix: [N] or (params) following a declarator core */
Type *type_suffix(Type *ty) {
    if (tk_eat("(")) return func_params(ty);
    if (tk_eat("[")) {
        int len = 0;
        int have = 0;
        if (!tk_is("]")) { len = (int)const_expr(); have = 1; }
        tk_expect("]");
        ty = type_suffix(ty);
        Type *a = array_of(ty, len);
        if (!have) a->is_incomplete = 1;
        return a;
    }
    return ty;
}

Type *declarator(Type *ty, char **name) {
    while (tk_eat("*")) {
        ty = pointer_to(ty);
        while (tk_is_kw("const") || tk_is_kw("volatile") || tk_is_kw("restrict")) tk_next();
    }

    if (tk_eat("(")) {
        /* parenthesised declarator: parse a placeholder, then apply suffix */
        Token *start = TK;
        /* skip balanced parens to find the suffix */
        int depth = 1;
        Token *t = TK;
        while (t && depth) {
            if (t->kind == T_PUNCT && xstreq(t->str, "(")) depth++;
            else if (t->kind == T_PUNCT && xstreq(t->str, ")")) depth--;
            if (depth == 0) break;
            t = t->next;
        }
        /* t is the matching ')' */
        TK = t->next;
        Type *suffixed = type_suffix(ty);
        TK = start;
        Type *inner = declarator(suffixed, name);
        /* consume up to and including matching ')' */
        TK = t->next;
        /* re-apply suffix scan already consumed above; skip it now */
        /* need to also skip the already-parsed suffix tokens */
        /* Simpler: re-run type_suffix to advance TK */
        type_suffix(ty);
        return inner;
    }

    if (TK->kind == T_IDENT) {
        *name = TK->str;
        tk_next();
    }
    return type_suffix(ty);
}

/* abstract declarator for casts / sizeof(type) */
Type *abstract_declarator(Type *ty) {
    while (tk_eat("*")) {
        ty = pointer_to(ty);
        while (tk_is_kw("const") || tk_is_kw("volatile") || tk_is_kw("restrict")) tk_next();
    }
    if (tk_eat("(")) {
        Token *start = TK;
        int depth = 1;
        Token *t = TK;
        while (t && depth) {
            if (t->kind == T_PUNCT && xstreq(t->str, "(")) depth++;
            else if (t->kind == T_PUNCT && xstreq(t->str, ")")) depth--;
            if (depth == 0) break;
            t = t->next;
        }
        TK = t->next;
        Type *suffixed = type_suffix(ty);
        TK = start;
        Type *inner = abstract_declarator(suffixed);
        TK = t->next;
        type_suffix(ty);
        return inner;
    }
    return type_suffix(ty);
}

Type *typename_(void) {
    int sc;
    Type *ty = declspec(&sc);
    return abstract_declarator(ty);
}

Type *struct_union_type(int kind) {
    char *tag = 0;
    if (TK->kind == T_IDENT) { tag = TK->str; tk_next(); }

    if (!tk_is("{")) {
        if (!tag) die_at(TK->line, "expected struct/union body or tag", 0);
        Type *t = find_tag(tag);
        if (t) return t;
        /* forward declaration */
        t = new_type(kind, 0, 1);
        t->tag = tag;
        t->is_incomplete = 1;
        ScopeVar *sv = push_scopevar(tag);
        sv->tag = t;
        return t;
    }

    Type *ty;
    Type *existing = tag ? find_tag_cur(tag) : 0;
    if (existing) ty = existing;
    else {
        ty = new_type(kind, 0, 1);
        ty->tag = tag;
        if (tag) { ScopeVar *sv = push_scopevar(tag); sv->tag = ty; }
    }

    tk_expect("{");
    Member head; head.next = 0;
    Member *cur = &head;
    int offset = 0;
    int maxalign = 1;
    while (!tk_eat("}")) {
        int sc;
        Type *bt = declspec(&sc);
        int first = 1;
        while (!tk_eat(";")) {
            if (!first) tk_expect(",");
            first = 0;
            char *nm = 0;
            Type *mt = declarator(bt, &nm);
            Member *m = xalloc(sizeof(Member));
            m->name = nm;
            m->ty = mt;
            if (mt->align > maxalign) maxalign = mt->align;
            if (kind == TY_STRUCT) {
                offset = align_to(offset, mt->align);
                m->offset = offset;
                offset += mt->size;
            } else {
                m->offset = 0;
                if (mt->size > offset) offset = mt->size;
            }
            cur->next = m;
            cur = m;
        }
    }
    ty->members = head.next;
    ty->align = maxalign;
    ty->size = align_to(offset, maxalign);
    ty->is_incomplete = 0;
    return ty;
}

Type *enum_type(void) {
    char *tag = 0;
    if (TK->kind == T_IDENT) { tag = TK->str; tk_next(); }
    if (!tk_is("{")) {
        if (tag) {
            Type *t = find_tag(tag);
            if (t) return t;
        }
        return ty_int;
    }
    tk_expect("{");
    long val = 0;
    while (!tk_eat("}")) {
        char *nm = tk_ident();
        if (tk_eat("=")) val = const_expr();
        Obj *o = new_global(nm, ty_int, OBJ_ENUM);
        o->enum_val = val;
        o->is_definition = 1;
        val++;
        if (!tk_eat(",")) { tk_expect("}"); break; }
    }
    Type *t = new_type(TY_INT, 4, 4);
    if (tag) { ScopeVar *sv = push_scopevar(tag); sv->tag = t; }
    return t;
}

/* ------------------------------------------------------------------ *
 *  expressions
 * ------------------------------------------------------------------ */
Node *funcall(Node *fn);

Node *primary(void) {
    int line = TK->line;

    if (tk_eat("(")) {
        if (tk_is("{")) {
            /* GNU statement expression */
            Node *n = new_node(ND_STMT_EXPR, line);
            n->body = compound_stmt()->body;
            tk_expect(")");
            return n;
        }
        Node *n = expr();
        tk_expect(")");
        return n;
    }

    if (tk_is_kw("sizeof")) {
        tk_next();
        if (tk_is("(") && TK->next && (TK->next->kind == T_KW || (TK->next->kind == T_IDENT && find_typedef(TK->next->str)))) {
            tk_next();
            Type *ty = typename_();
            tk_expect(")");
            Node *n = new_num(ty->size, line);
            n->ty = ty_uint;
            return n;
        }
        Node *e = cast_expr();
        add_type(e);
        Node *n = new_num(e->ty->size, line);
        n->ty = ty_uint;
        return n;
    }

    if (TK->kind == T_NUM) {
        long v = TK->val;
        tk_next();
        return new_num(v, line);
    }
    if (TK->kind == T_CHAR) {
        long v = TK->val;
        tk_next();
        Node *n = new_num(v, line);
        n->ty = ty_int;
        return n;
    }
    if (TK->kind == T_STR) {
        Str *s = xalloc(sizeof(Str));
        s->data = TK->sval;
        s->len = TK->slen + 1;   /* include NUL */
        if (!g_strs) g_strs = s; else g_strs_tail->next = s;
        g_strs_tail = s;
        tk_next();
        Node *n = new_node(ND_VAR, line);
        n->str = s;
        n->ty = array_of(ty_char, s->len);
        return n;
    }
    if (TK->kind == T_IDENT) {
        char *name = TK->str;
        tk_next();
        Obj *o = find_var(name);
        if (!o) {
            /* implicit declaration: treat as extern int function */
            if (tk_is("(")) {
                Type *fty = func_type(ty_int);
                o = new_global(xstrdup(name), fty, OBJ_FUNC);
                o->is_extern = 1;
            } else {
                die_at(line, "undefined symbol: ", name);
            }
        }
        if (o->kind == OBJ_ENUM) {
            Node *n = new_num(o->enum_val, line);
            n->ty = ty_int;
            return n;
        }
        Node *n = new_var_node(o, line);
        return n;
    }

    die_at(line, "unexpected token in expression", 0);
    return 0;
}

Node *postfix(void) {
    Node *n = primary();
    for (;;) {
        int line = TK->line;
        if (tk_eat("(")) {
            n = funcall(n);
            continue;
        }
        if (tk_eat("[")) {
            Node *idx = expr();
            tk_expect("]");
            Node *add = ptr_add(n, idx, line);
            n = new_unary(ND_DEREF, add, line);
            continue;
        }
        if (tk_eat(".")) {
            Node *m = new_unary(ND_MEMBER, n, line);
            m->member_name = tk_ident();
            n = m;
            continue;
        }
        if (tk_eat("->")) {
            Node *deref = new_unary(ND_DEREF, n, line);
            Node *m = new_unary(ND_MEMBER, deref, line);
            m->member_name = tk_ident();
            n = m;
            continue;
        }
        if (tk_is("++") || tk_is("--")) {
            int isinc = tk_is("++");
            tk_next();
            /* x++  ->  (tmp = &x, *tmp = *tmp + 1, *tmp - 1) : do it simply via
               (x += 1) - 1  with pointer scaling handled by ptr_add */
            add_type(n);
            Node *one = new_num(1, line);
            Node *pre = isinc ? ptr_add(n, one, line) : ptr_sub(n, one, line);
            Node *asn = new_binary(ND_ASSIGN, n, pre, line);
            add_type(asn);
            Node *back = isinc ? ptr_sub(asn, new_num(1, line), line)
                               : ptr_add(asn, new_num(1, line), line);
            n = back;
            continue;
        }
        return n;
    }
}

Node *funcall(Node *fn) {
    int line = TK->line;
    add_type(fn);
    Type *fty = fn->ty;
    if (fty->kind == TY_PTR) fty = fty->base;

    Node head; head.next = 0;
    Node *cur = &head;
    while (!tk_eat(")")) {
        if (cur != &head) tk_expect(",");
        Node *a = assign();
        add_type(a);
        if (a->ty->kind == TY_ARRAY || a->ty->kind == TY_FUNC)
            a = new_cast(a, decay(a->ty));
        cur->next = a;
        cur = a;
    }

    Node *n = new_node(ND_CALL, line);
    n->fn = fn;
    n->args = head.next;
    if (fty->kind == TY_FUNC) n->ty = fty->base;
    else n->ty = ty_int;
    if (n->ty->kind == TY_ARRAY) n->ty = decay(n->ty);
    return n;
}

Node *unary(void) {
    int line = TK->line;
    if (tk_eat("+")) return cast_expr();
    if (tk_eat("-")) return new_unary(ND_NEG, cast_expr(), line);
    if (tk_eat("!")) return new_unary(ND_NOT, cast_expr(), line);
    if (tk_eat("~")) return new_unary(ND_BITNOT, cast_expr(), line);
    if (tk_eat("&")) return new_unary(ND_ADDR, cast_expr(), line);
    if (tk_eat("*")) return new_unary(ND_DEREF, cast_expr(), line);
    if (tk_is("++") || tk_is("--")) {
        int isinc = tk_is("++");
        tk_next();
        Node *e = unary();
        Node *one = new_num(1, line);
        Node *pre = isinc ? ptr_add(e, one, line) : ptr_sub(e, one, line);
        return new_binary(ND_ASSIGN, e, pre, line);
    }
    return postfix();
}

Node *cast_expr(void) {
    if (tk_is("(") && TK->next &&
        (TK->next->kind == T_KW || (TK->next->kind == T_IDENT && find_typedef(TK->next->str)))) {
        int line = TK->line;
        tk_next();
        Type *ty = typename_();
        tk_expect(")");
        if (tk_is("{")) {
            /* compound literal - not supported */
            die_at(line, "compound literals are not supported", 0);
        }
        Node *n = new_node(ND_CAST, line);
        n->lhs = cast_expr();
        n->ty = ty;
        return n;
    }
    return unary();
}

Node *mul(void) {
    Node *n = cast_expr();
    for (;;) {
        int line = TK->line;
        if (tk_eat("*")) n = new_binary(ND_MUL, n, cast_expr(), line);
        else if (tk_eat("/")) n = new_binary(ND_DIV, n, cast_expr(), line);
        else if (tk_eat("%")) n = new_binary(ND_MOD, n, cast_expr(), line);
        else return n;
    }
}

Node *add(void) {
    Node *n = mul();
    for (;;) {
        int line = TK->line;
        if (tk_eat("+")) n = ptr_add(n, mul(), line);
        else if (tk_eat("-")) n = ptr_sub(n, mul(), line);
        else return n;
    }
}

Node *shift(void) {
    Node *n = add();
    for (;;) {
        int line = TK->line;
        if (tk_eat("<<")) n = new_binary(ND_SHL, n, add(), line);
        else if (tk_eat(">>")) n = new_binary(ND_SHR, n, add(), line);
        else return n;
    }
}

Node *relational(void) {
    Node *n = shift();
    for (;;) {
        int line = TK->line;
        if (tk_eat("<")) n = new_binary(ND_LT, n, shift(), line);
        else if (tk_eat("<=")) n = new_binary(ND_LE, n, shift(), line);
        else if (tk_eat(">")) n = new_binary(ND_LT, shift(), n, line);
        else if (tk_eat(">=")) n = new_binary(ND_LE, shift(), n, line);
        else return n;
    }
}

Node *equality(void) {
    Node *n = relational();
    for (;;) {
        int line = TK->line;
        if (tk_eat("==")) n = new_binary(ND_EQ, n, relational(), line);
        else if (tk_eat("!=")) n = new_binary(ND_NE, n, relational(), line);
        else return n;
    }
}

Node *bitand_(void) {
    Node *n = equality();
    while (tk_is("&") && !tk_is("&&")) { int line = TK->line; tk_next(); n = new_binary(ND_BITAND, n, equality(), line); }
    return n;
}
Node *bitxor_(void) {
    Node *n = bitand_();
    while (tk_is("^")) { int line = TK->line; tk_next(); n = new_binary(ND_BITXOR, n, bitand_(), line); }
    return n;
}
Node *bitor_(void) {
    Node *n = bitxor_();
    while (tk_is("|") && !tk_is("||")) { int line = TK->line; tk_next(); n = new_binary(ND_BITOR, n, bitxor_(), line); }
    return n;
}
Node *logand(void) {
    Node *n = bitor_();
    while (tk_eat("&&")) { int line = TK->line; n = new_binary(ND_LOGAND, n, bitor_(), line); }
    return n;
}
Node *logor(void) {
    Node *n = logand();
    while (tk_eat("||")) { int line = TK->line; n = new_binary(ND_LOGOR, n, logand(), line); }
    return n;
}

Node *conditional(void) {
    Node *c = logor();
    int line = TK->line;
    if (!tk_eat("?")) return c;
    Node *n = new_node(ND_COND, line);
    n->cond = c;
    n->then = expr();
    tk_expect(":");
    n->els = conditional();
    return n;
}

Node *to_assign(int op, Node *lhs, Node *rhs, int line) {
    /* lhs op= rhs   ->   lhs = lhs op rhs  (lhs evaluated once is acceptable
       here since lhs is a simple lvalue in practice for this compiler) */
    Node *bin;
    switch (op) {
    case ND_ADD: bin = ptr_add(lhs, rhs, line); break;
    case ND_SUB: bin = ptr_sub(lhs, rhs, line); break;
    default: bin = new_binary(op, lhs, rhs, line); break;
    }
    return new_binary(ND_ASSIGN, lhs, bin, line);
}

Node *assign(void) {
    Node *n = conditional();
    int line = TK->line;
    if (tk_eat("=")) return new_binary(ND_ASSIGN, n, assign(), line);
    if (tk_eat("+=")) return to_assign(ND_ADD, n, assign(), line);
    if (tk_eat("-=")) return to_assign(ND_SUB, n, assign(), line);
    if (tk_eat("*=")) return to_assign(ND_MUL, n, assign(), line);
    if (tk_eat("/=")) return to_assign(ND_DIV, n, assign(), line);
    if (tk_eat("%=")) return to_assign(ND_MOD, n, assign(), line);
    if (tk_eat("&=")) return to_assign(ND_BITAND, n, assign(), line);
    if (tk_eat("|=")) return to_assign(ND_BITOR, n, assign(), line);
    if (tk_eat("^=")) return to_assign(ND_BITXOR, n, assign(), line);
    if (tk_eat("<<=")) return to_assign(ND_SHL, n, assign(), line);
    if (tk_eat(">>=")) return to_assign(ND_SHR, n, assign(), line);
    return n;
}

Node *expr(void) {
    Node *n = assign();
    int line = TK->line;
    while (tk_eat(",")) {
        n = new_binary(ND_COMMA, n, assign(), line);
    }
    return n;
}

long eval_const(Node *n);

long const_expr(void) {
    Node *n = conditional();
    add_type(n);
    return eval_const(n);
}

long eval_const(Node *n) {
    switch (n->kind) {
    case ND_NUM: return n->val;
    case ND_ADD: return eval_const(n->lhs) + eval_const(n->rhs);
    case ND_SUB: return eval_const(n->lhs) - eval_const(n->rhs);
    case ND_MUL: return eval_const(n->lhs) * eval_const(n->rhs);
    case ND_DIV: return eval_const(n->lhs) / eval_const(n->rhs);
    case ND_MOD: return eval_const(n->lhs) % eval_const(n->rhs);
    case ND_NEG: return -eval_const(n->lhs);
    case ND_BITNOT: return ~eval_const(n->lhs);
    case ND_NOT: return !eval_const(n->lhs);
    case ND_BITAND: return eval_const(n->lhs) & eval_const(n->rhs);
    case ND_BITOR: return eval_const(n->lhs) | eval_const(n->rhs);
    case ND_BITXOR: return eval_const(n->lhs) ^ eval_const(n->rhs);
    case ND_SHL: return eval_const(n->lhs) << eval_const(n->rhs);
    case ND_SHR: return eval_const(n->lhs) >> eval_const(n->rhs);
    case ND_EQ: return eval_const(n->lhs) == eval_const(n->rhs);
    case ND_NE: return eval_const(n->lhs) != eval_const(n->rhs);
    case ND_LT: return eval_const(n->lhs) < eval_const(n->rhs);
    case ND_LE: return eval_const(n->lhs) <= eval_const(n->rhs);
    case ND_LOGAND: return eval_const(n->lhs) && eval_const(n->rhs);
    case ND_LOGOR: return eval_const(n->lhs) || eval_const(n->rhs);
    case ND_COND: return eval_const(n->cond) ? eval_const(n->then) : eval_const(n->els);
    case ND_COMMA: return eval_const(n->rhs);
    case ND_CAST: return eval_const(n->lhs);
    }
    die_at(n->line, "not a constant expression", 0);
    return 0;
}

/* ------------------------------------------------------------------ *
 *  statements / local declarations
 * ------------------------------------------------------------------ */
Obj *g_cur_switch_obj;
CaseLabel **g_cur_case_tail;
int g_brk_label, g_cont_label;
int g_switch_default_label;
int g_switch_has_default;
CaseLabel *g_switch_cases;
CaseLabel **g_switch_cases_tail;

void alloc_local(Obj *v) {
    Type *t = v->ty;
    int a = t->align < 4 ? 4 : t->align;
    if (t->kind == TY_STRUCT || t->kind == TY_UNION || t->kind == TY_ARRAY) a = t->align < 4 ? 4 : t->align;
    g_cur_fn->stack_size = align_to(g_cur_fn->stack_size + t->size, a);
    v->offset = -g_cur_fn->stack_size;
}

Node *lvar_init(Obj *var, int line);

Node *declaration(void) {
    int sc;
    Type *base = declspec(&sc);

    Node head; head.next = 0;
    Node *cur = &head;
    int first = 1;

    while (!tk_eat(";")) {
        if (!first) tk_expect(",");
        first = 0;
        char *name = 0;
        Type *ty = declarator(base, &name);
        if (!name) die_at(TK->line, "expected a variable name", 0);

        if (sc & SC_TYPEDEF) {
            push_scopevar(name)->type_def = ty;
            continue;
        }

        if (ty->kind == TY_FUNC) {
            /* local function declaration (prototype) */
            Obj *o = find_var(name);
            if (!o) { o = new_global(xstrdup(name), ty, OBJ_FUNC); o->is_extern = 1; }
            continue;
        }

        if (sc & SC_STATIC) {
            /* function-local static: emit as a global with a mangled name */
            Obj *g = new_global(0, ty, OBJ_VAR);
            g->name = name;
            g->is_static = 1;
            g->is_definition = 1;
            push_scopevar(name)->var = g;
            if (tk_eat("=")) {
                /* only constant initializers for statics */
                die_at(TK->line, "initializer for a local static is not supported", 0);
            }
            continue;
        }

        if (sc & SC_EXTERN) {
            Obj *o = new_global(xstrdup(name), ty, OBJ_VAR);
            o->is_extern = 1;
            continue;
        }

        Obj *v = new_local(name, ty);
        alloc_local(v);

        if (tk_eat("=")) {
            Node *init = lvar_init(v, TK->line);
            cur->next = init;
            while (cur->next) cur = cur->next;
        }
    }
    Node *blk = new_node(ND_BLOCK, 0);
    blk->body = head.next;
    return blk;
}

/* Lower an initializer into a list of assignment expr-statements. */
Node *make_assign_stmt(Node *lhs, Node *rhs, int line) {
    Node *a = new_binary(ND_ASSIGN, lhs, rhs, line);
    Node *s = new_node(ND_EXPR_STMT, line);
    s->lhs = a;
    return s;
}

Node *lvar_init(Obj *var, int line) {
    Type *ty = var->ty;
    Node head; head.next = 0;
    Node *cur = &head;

    if (ty->kind == TY_ARRAY && ty->base->kind == TY_CHAR && TK->kind == T_STR) {
        /* char x[] = "..."  */
        Str *s = xalloc(sizeof(Str));
        s->data = TK->sval;
        s->len = TK->slen + 1;
        tk_next();
        int n = s->len;
        if (ty->is_incomplete) { ty->array_len = n; ty->size = n; var->offset = 0; alloc_local(var); }
        int lim = ty->array_len < n ? ty->array_len : n;
        for (int i = 0; i < lim; i++) {
            Node *elem = new_unary(ND_DEREF,
                ptr_add(new_var_node(var, line), new_num(i, line), line), line);
            cur->next = make_assign_stmt(elem, new_num((unsigned char)s->data[i], line), line);
            cur = cur->next;
        }
        return head.next;
    }

    if (tk_is("{")) {
        tk_next();
        if (ty->kind == TY_ARRAY) {
            int i = 0;
            if (ty->is_incomplete) {
                /* count elements first via a token scan is complex; require size */
            }
            while (!tk_eat("}")) {
                if (i) tk_expect(",");
                if (tk_is("}")) break;
                Node *elem = new_unary(ND_DEREF,
                    ptr_add(new_var_node(var, line), new_num(i, line), line), line);
                if (tk_is("{")) {
                    /* nested: only supported for arrays of scalars flattened -- skip */
                    die_at(TK->line, "nested brace initializer not supported", 0);
                }
                Node *v = assign();
                cur->next = make_assign_stmt(elem, v, line);
                cur = cur->next;
                i++;
            }
            return head.next;
        }
        if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
            Member *m = ty->members;
            int idx = 0;
            while (!tk_eat("}")) {
                if (idx) tk_expect(",");
                if (tk_is("}")) break;
                if (!m) die_at(TK->line, "too many initializers", 0);
                Node *mem = new_unary(ND_MEMBER, new_var_node(var, line), line);
                mem->member_name = m->name;
                Node *v = assign();
                cur->next = make_assign_stmt(mem, v, line);
                cur = cur->next;
                m = m->next;
                idx++;
            }
            return head.next;
        }
        /* scalar in braces */
        Node *v = assign();
        tk_expect("}");
        return make_assign_stmt(new_var_node(var, line), v, line);
    }

    /* scalar initializer */
    Node *v = assign();
    return make_assign_stmt(new_var_node(var, line), v, line);
}

Node *stmt(void) {
    int line = TK->line;

    if (tk_eat("return")) {
        Node *n = new_node(ND_RETURN, line);
        if (!tk_is(";")) n->lhs = expr();
        tk_expect(";");
        return n;
    }
    if (tk_eat("if")) {
        Node *n = new_node(ND_IF, line);
        tk_expect("(");
        n->cond = expr();
        tk_expect(")");
        n->then = stmt();
        if (tk_eat("else")) n->els = stmt();
        return n;
    }
    if (tk_eat("while")) {
        Node *n = new_node(ND_FOR, line);
        tk_expect("(");
        n->cond = expr();
        tk_expect(")");
        int sb = g_brk_label, sc2 = g_cont_label;
        n->brk_label = g_brk_label = new_label();
        n->cont_label = g_cont_label = new_label();
        n->then = stmt();
        g_brk_label = sb; g_cont_label = sc2;
        return n;
    }
    if (tk_eat("for")) {
        Node *n = new_node(ND_FOR, line);
        tk_expect("(");
        enter_scope();
        if (!tk_eat(";")) {
            if (is_typename()) n->init = declaration();
            else { Node *e = new_node(ND_EXPR_STMT, line); e->lhs = expr(); tk_expect(";"); n->init = e; }
        }
        if (!tk_is(";")) n->cond = expr();
        tk_expect(";");
        if (!tk_is(")")) n->inc = expr();
        tk_expect(")");
        int sb = g_brk_label, sc2 = g_cont_label;
        n->brk_label = g_brk_label = new_label();
        n->cont_label = g_cont_label = new_label();
        n->then = stmt();
        g_brk_label = sb; g_cont_label = sc2;
        leave_scope();
        return n;
    }
    if (tk_eat("do")) {
        Node *n = new_node(ND_DO, line);
        int sb = g_brk_label, sc2 = g_cont_label;
        n->brk_label = g_brk_label = new_label();
        n->cont_label = g_cont_label = new_label();
        n->then = stmt();
        g_brk_label = sb; g_cont_label = sc2;
        tk_expect("while");
        tk_expect("(");
        n->cond = expr();
        tk_expect(")");
        tk_expect(";");
        return n;
    }
    if (tk_eat("switch")) {
        Node *n = new_node(ND_SWITCH, line);
        tk_expect("(");
        n->cond = expr();
        tk_expect(")");
        CaseLabel *save_cases = g_switch_cases;
        CaseLabel **save_tail = g_switch_cases_tail;
        int save_def = g_switch_default_label;
        int save_hasdef = g_switch_has_default;
        int sb = g_brk_label;
        g_switch_cases = 0;
        g_switch_cases_tail = &g_switch_cases;
        g_switch_default_label = 0;
        g_switch_has_default = 0;
        n->brk_label = g_brk_label = new_label();
        n->then = stmt();
        n->cases = g_switch_cases;
        n->cont_label = g_switch_default_label;   /* reuse cont_label as default */
        n->val = g_switch_has_default;            /* has-default flag */
        g_switch_cases = save_cases;
        g_switch_cases_tail = save_tail;
        g_switch_default_label = save_def;
        g_switch_has_default = save_hasdef;
        g_brk_label = sb;
        return n;
    }
    if (tk_eat("case")) {
        long v = const_expr();
        tk_expect(":");
        CaseLabel *c = xalloc(sizeof(CaseLabel));
        c->val = v;
        c->label = new_label();
        *g_switch_cases_tail = c;
        g_switch_cases_tail = &c->next;
        Node *n = new_node(ND_CASE, line);
        n->brk_label = c->label;
        n->lhs = stmt();
        return n;
    }
    if (tk_eat("default")) {
        tk_expect(":");
        g_switch_default_label = new_label();
        g_switch_has_default = 1;
        Node *n = new_node(ND_CASE, line);
        n->brk_label = g_switch_default_label;
        n->lhs = stmt();
        return n;
    }
    if (tk_eat("break")) {
        tk_expect(";");
        Node *n = new_node(ND_BREAK, line);
        n->brk_label = g_brk_label;
        return n;
    }
    if (tk_eat("continue")) {
        tk_expect(";");
        Node *n = new_node(ND_CONTINUE, line);
        n->cont_label = g_cont_label;
        return n;
    }
    if (tk_eat("goto")) {
        Node *n = new_node(ND_GOTO, line);
        n->label_name = tk_ident();
        tk_expect(";");
        return n;
    }
    if (TK->kind == T_IDENT && TK->next && TK->next->kind == T_PUNCT && xstreq(TK->next->str, ":")) {
        Node *n = new_node(ND_LABEL, line);
        n->label_name = TK->str;
        tk_next();
        tk_next();
        n->lhs = stmt();
        return n;
    }
    if (tk_is("{")) return compound_stmt();
    if (tk_eat(";")) return new_node(ND_NULL, line);

    Node *n = new_node(ND_EXPR_STMT, line);
    n->lhs = expr();
    tk_expect(";");
    return n;
}

Node *compound_stmt(void) {
    int line = TK->line;
    tk_expect("{");
    Node *n = new_node(ND_BLOCK, line);
    Node head; head.next = 0;
    Node *cur = &head;
    enter_scope();
    while (!tk_eat("}")) {
        if (is_typename()) cur->next = declaration();
        else cur->next = stmt();
        while (cur->next) cur = cur->next;
        add_type(cur);
    }
    leave_scope();
    n->body = head.next;
    return n;
}

/* ------------------------------------------------------------------ *
 *  top level
 * ------------------------------------------------------------------ */
void global_var(Obj *o, int has_init);
void gen_text(void);
void emit_elf(char *outpath);
char *g_outpath;

void function(Obj *fn, Type *fty) {
    fn->kind = OBJ_FUNC;
    fn->is_variadic = fty->is_variadic;

    enter_scope();
    g_cur_fn = fn;
    fn->stack_size = 0;

    /* bind params at positive ebp offsets */
    Obj phead; phead.next = 0;
    Obj *pcur = &phead;
    int poff = 8;
    for (Type *p = fty->params; p; p = p->next) {
        char *nm = p->tag;
        Obj *pv = xalloc(sizeof(Obj));
        pv->name = nm;
        pv->ty = p;
        pv->kind = OBJ_VAR;
        pv->is_local = 1;
        pv->offset = poff;
        poff += 4;
        if (nm) push_scopevar(nm)->var = pv;
        pcur->next = pv;
        pcur = pv;
    }
    fn->params = phead.next;

    if (tk_is(";")) {
        tk_next();
        /* prototype: leave any earlier definition intact */
        leave_scope();
        g_cur_fn = 0;
        return;
    }

    if (fn->is_definition)
        die_at(TK->line, "redefinition of function: ", fn->name);
    fn->is_definition = 1;
    fn->body = compound_stmt();
    add_type(fn->body);
    fn->stack_size = align_to(fn->stack_size, 16);
    leave_scope();
    g_cur_fn = 0;
}

void parse_toplevel(void) {
    while (TK->kind != T_EOF) {
        int sc;
        Type *base = declspec(&sc);

        if (tk_eat(";")) continue;   /* bare struct/enum decl */

        int first = 1;
        while (!tk_eat(";")) {
            if (!first) tk_expect(",");
            first = 0;

            char *name = 0;
            Type *ty = declarator(base, &name);

            if (sc & SC_TYPEDEF) {
                if (name) push_scopevar(name)->type_def = ty;
                continue;
            }
            if (!name) die_at(TK->line, "expected a name at top level", 0);

            if (ty->kind == TY_FUNC) {
                Obj *o = find_var(name);
                if (!o || o->kind != OBJ_FUNC) {
                    o = new_global(xstrdup(name), ty, OBJ_FUNC);
                }
                o->ty = ty;
                if (sc & SC_STATIC) o->is_static = 1;
                if (sc & SC_EXTERN) o->is_extern = 1;
                function(o, ty);
                break;   /* function definition ends the declarator list */
            }

            /* global variable */
            Obj *o = find_var(name);
            if (!o) o = new_global(xstrdup(name), ty, OBJ_VAR);
            o->ty = ty;
            if (sc & SC_STATIC) o->is_static = 1;
            if (sc & SC_EXTERN) o->is_extern = 1;

            if (tk_eat("=")) {
                o->is_definition = 1;
                global_var(o, 1);
            } else {
                o->is_definition = o->is_definition || !(sc & SC_EXTERN);
            }
        }
    }
}

void gen_program(void) {
    parse_toplevel();
    gen_text();
    emit_elf(g_outpath);
}

/* ================================================================== *
 *  CODEGEN  (pass 2)  -- i386, stack-machine, then one PT_LOAD ELF
 * ================================================================== */

enum { LOAD_ADDR = 0x800000, HDR_SIZE = 84 };   /* 52 ehdr + 32 phdr */

unsigned char *g_code; int g_code_len; int g_code_cap;
unsigned char *g_data; int g_data_len; int g_data_cap;
int g_data_off;   /* file offset / vaddr offset of data section (from LOAD) */

int *g_label_off; int g_label_cap;

enum {
    FIX_LABEL_REL,     /* code[loc] = label_off[t] - (loc+4) */
    FIX_CALL,          /* code[loc] = obj->code_off - (loc+4) */
    FIX_ABS_OBJ,       /* code[loc] = LOAD + (obj addr) + addend */
    FIX_ABS_STR,       /* code[loc] = LOAD + g_data_off + str->data_off + addend */
    FIX_DATA_OBJ,      /* data[loc] = LOAD + (obj addr) + addend */
    FIX_DATA_STR       /* data[loc] = LOAD + g_data_off + str->data_off + addend */
};

typedef struct Fix Fix;
struct Fix {
    int kind;
    int loc;           /* code offset; for FIX_DATA_*, offset within `owner` */
    int t;             /* label id */
    Obj *obj;          /* target symbol */
    Str *str;          /* target string */
    Obj *owner;        /* FIX_DATA_*: the global whose init image holds `loc` */
    int addend;
    Fix *next;
};
Fix *g_fixes;
Fix *g_fixes_tail;

void add_fix(int kind, int loc, Obj *obj, Str *str, int t, int addend) {
    Fix *f = xalloc(sizeof(Fix));
    f->kind = kind; f->loc = loc; f->obj = obj; f->str = str; f->t = t; f->addend = addend;
    if (!g_fixes) g_fixes = f; else g_fixes_tail->next = f;
    g_fixes_tail = f;
}

void add_data_fix(int kind, Obj *owner, int off, Obj *obj, Str *str, int addend) {
    Fix *f = xalloc(sizeof(Fix));
    f->kind = kind; f->loc = off; f->owner = owner;
    f->obj = obj; f->str = str; f->addend = addend;
    if (!g_fixes) g_fixes = f; else g_fixes_tail->next = f;
    g_fixes_tail = f;
}

void emit(int b) {
    if (g_code_len + 1 > g_code_cap) {
        g_code_cap = g_code_cap ? g_code_cap * 2 : 4096;
        g_code = xrealloc(g_code, g_code_cap);
    }
    g_code[g_code_len++] = (unsigned char)(b & 0xff);
}
void emit4(int v) {
    emit(v); emit(v >> 8); emit(v >> 16); emit(v >> 24);
}
void emitb(char *p, int n) { for (int i = 0; i < n; i++) emit(p[i]); }

void patch4(unsigned char *buf, int loc, int v) {
    buf[loc] = v & 0xff;
    buf[loc + 1] = (v >> 8) & 0xff;
    buf[loc + 2] = (v >> 16) & 0xff;
    buf[loc + 3] = (v >> 24) & 0xff;
}

void ensure_label_cap(int id) {
    if (id + 1 > g_label_cap) {
        int nc = g_label_cap ? g_label_cap * 2 : 256;
        while (nc < id + 1) nc *= 2;
        int *nn = xalloc(nc * (int)sizeof(int));
        for (int i = 0; i < g_label_cap; i++) nn[i] = g_label_off[i];
        g_label_off = nn;
        g_label_cap = nc;
    }
}
void place_label(int id) {
    ensure_label_cap(id);
    g_label_off[id] = g_code_len;
}
void emit_jmp(int id) {
    emit(0xE9); emit4(0);
    add_fix(FIX_LABEL_REL, g_code_len - 4, 0, 0, id, 0);
}
void emit_jcc(int op, int id) {
    emit(0x0F); emit(op); emit4(0);
    add_fix(FIX_LABEL_REL, g_code_len - 4, 0, 0, id, 0);
}
void emit_test_jz(int id) { emit(0x85); emit(0xC0); emit_jcc(0x84, id); }
void emit_test_jnz(int id) { emit(0x85); emit(0xC0); emit_jcc(0x85, id); }

void emit_mov_eax_imm(int v) { emit(0xB8); emit4(v); }
void emit_push_eax(void) { emit(0x50); }
void emit_pop_eax(void) { emit(0x58); }
void emit_pop_ecx(void) { emit(0x59); }

void emit_call_obj(Obj *fn) {
    emit(0xE8); emit4(0);
    add_fix(FIX_CALL, g_code_len - 4, fn, 0, 0, 0);
}
void emit_mov_eax_obj(Obj *o, int addend) {
    emit(0xB8); emit4(0);
    add_fix(FIX_ABS_OBJ, g_code_len - 4, o, 0, 0, addend);
}
void emit_mov_eax_str(Str *s, int addend) {
    emit(0xB8); emit4(0);
    add_fix(FIX_ABS_STR, g_code_len - 4, 0, s, 0, addend);
}
void emit_lea_ebp(int off) {   /* lea eax,[ebp+off] */
    emit(0x8D); emit(0x85); emit4(off);
}

Obj *g_gen_fn;
int g_epilogue;

/* per-function named-label table */
typedef struct NLbl NLbl;
struct NLbl { char *name; int id; NLbl *next; };
NLbl *g_named;

int named_label(char *name) {
    for (NLbl *p = g_named; p; p = p->next)
        if (xstreq(p->name, name)) return p->id;
    NLbl *p = xalloc(sizeof(NLbl));
    p->name = name;
    p->id = new_label();
    p->next = g_named;
    g_named = p;
    return p->id;
}

void gen_expr(Node *n);
void gen_stmt(Node *n);

void load_val(Type *ty) {
    /* eax holds an address; load the value per type */
    if (ty->kind == TY_ARRAY || ty->kind == TY_STRUCT || ty->kind == TY_UNION || ty->kind == TY_FUNC)
        return;   /* aggregate: value is the address */
    if (ty->size == 1) {
        if (ty->is_unsigned) { emit(0x0F); emit(0xB6); emit(0x00); }   /* movzx eax,byte[eax] */
        else { emit(0x0F); emit(0xBE); emit(0x00); }                    /* movsx eax,byte[eax] */
    } else if (ty->size == 2) {
        if (ty->is_unsigned) { emit(0x0F); emit(0xB7); emit(0x00); }
        else { emit(0x0F); emit(0xBF); emit(0x00); }
    } else {
        emit(0x8B); emit(0x00);   /* mov eax,[eax] */
    }
}

void store_val(Type *ty) {
    /* [ecx] = eax, per type. eax preserved as the resulting value. */
    if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
        /* not reached: struct assign handled separately */
        return;
    }
    if (ty->size == 1) { emit(0x88); emit(0x01); }        /* mov [ecx],al */
    else if (ty->size == 2) { emit(0x66); emit(0x89); emit(0x01); }  /* mov [ecx],ax */
    else { emit(0x89); emit(0x01); }                       /* mov [ecx],eax */
}

void gen_addr(Node *n) {
    if (!n->ty) add_type(n);
    switch (n->kind) {
    case ND_VAR:
        if (n->str) { emit_mov_eax_str(n->str, 0); return; }
        if (n->var->is_local) { emit_lea_ebp(n->var->offset); return; }
        emit_mov_eax_obj(n->var, 0);
        return;
    case ND_DEREF:
        gen_expr(n->lhs);
        return;
    case ND_MEMBER:
        gen_addr(n->lhs);
        if (n->member->offset) { emit(0x05); emit4(n->member->offset); }  /* add eax,imm32 */
        return;
    case ND_COMMA:
        gen_expr(n->lhs);
        gen_addr(n->rhs);
        return;
    }
    die_at(n->line, "not an lvalue", 0);
}

void gen_struct_copy(int size) {
    /* dst addr on stack top, src addr in eax */
    emit(0x56);                       /* push esi */
    emit(0x57);                       /* push edi */
    emit(0x89); emit(0xC6);           /* mov esi,eax  (src) */
    emit(0x8B); emit(0x7C); emit(0x24); emit(0x08);   /* mov edi,[esp+8]  (dst) */
    emit(0xB9); emit4(size);          /* mov ecx,size */
    emit(0xFC);                       /* cld */
    emit(0xF3); emit(0xA4);           /* rep movsb */
    emit(0x5F);                       /* pop edi */
    emit(0x5E);                       /* pop esi */
    emit(0x58);                       /* pop eax  (dst -> result) */
}

int is_unsigned_cmp(Node *n) {
    Type *a = n->lhs->ty, *b = n->rhs->ty;
    if (is_pointer(a) || is_pointer(b)) return 1;
    if ((a && a->is_unsigned) || (b && b->is_unsigned)) return 1;
    return 0;
}

void gen_binop(Node *n) {
    gen_expr(n->lhs);
    emit_push_eax();
    gen_expr(n->rhs);
    emit(0x89); emit(0xC1);   /* mov ecx,eax */
    emit_pop_eax();            /* eax = lhs */

    int uns = (n->ty && n->ty->is_unsigned);
    switch (n->kind) {
    case ND_ADD: emit(0x01); emit(0xC8); break;                 /* add eax,ecx */
    case ND_SUB: emit(0x29); emit(0xC8); break;                 /* sub eax,ecx */
    case ND_MUL: emit(0x0F); emit(0xAF); emit(0xC1); break;     /* imul eax,ecx */
    case ND_DIV:
        if (uns) { emit(0x31); emit(0xD2); emit(0xF7); emit(0xF1); }   /* xor edx,edx; div ecx */
        else { emit(0x99); emit(0xF7); emit(0xF9); }                    /* cdq; idiv ecx */
        break;
    case ND_MOD:
        if (uns) { emit(0x31); emit(0xD2); emit(0xF7); emit(0xF1); }
        else { emit(0x99); emit(0xF7); emit(0xF9); }
        emit(0x89); emit(0xD0);   /* mov eax,edx */
        break;
    case ND_BITAND: emit(0x21); emit(0xC8); break;
    case ND_BITOR:  emit(0x09); emit(0xC8); break;
    case ND_BITXOR: emit(0x31); emit(0xC8); break;
    case ND_SHL: emit(0xD3); emit(0xE0); break;                 /* shl eax,cl */
    case ND_SHR:
        if (n->lhs->ty && n->lhs->ty->is_unsigned) { emit(0xD3); emit(0xE8); }  /* shr */
        else { emit(0xD3); emit(0xF8); }                                         /* sar */
        break;
    case ND_EQ: case ND_NE: case ND_LT: case ND_LE: {
        emit(0x39); emit(0xC8);   /* cmp eax,ecx */
        int u = is_unsigned_cmp(n);
        int op;
        if (n->kind == ND_EQ) op = 0x94;
        else if (n->kind == ND_NE) op = 0x95;
        else if (n->kind == ND_LT) op = u ? 0x92 : 0x9C;
        else op = u ? 0x96 : 0x9E;
        emit(0x0F); emit(op); emit(0xC0);         /* setcc al */
        emit(0x0F); emit(0xB6); emit(0xC0);       /* movzx eax,al */
        break;
    }
    }
}

void gen_cast(Node *n) {
    gen_expr(n->lhs);
    Type *to = n->ty;
    Type *from = n->lhs->ty;
    if (!to || to->kind == TY_VOID) return;
    if (to->kind == TY_BOOL) {
        emit(0x85); emit(0xC0);
        emit(0x0F); emit(0x95); emit(0xC0);
        emit(0x0F); emit(0xB6); emit(0xC0);
        return;
    }
    if (is_integer(to) && to->size == 1) {
        if (to->is_unsigned) { emit(0x0F); emit(0xB6); emit(0xC0); }   /* movzx eax,al */
        else { emit(0x0F); emit(0xBE); emit(0xC0); }                    /* movsx eax,al */
        return;
    }
    if (is_integer(to) && to->size == 2) {
        if (to->is_unsigned) { emit(0x0F); emit(0xB7); emit(0xC0); }
        else { emit(0x0F); emit(0xBF); emit(0xC0); }
        return;
    }
    (void)from;
}

void gen_expr(Node *n) {
    if (!n) return;
    if (!n->ty) add_type(n);
    switch (n->kind) {
    case ND_NUM:
        emit_mov_eax_imm((int)n->val);
        return;
    case ND_VAR:
        gen_addr(n);
        load_val(n->ty);
        return;
    case ND_ADDR:
        gen_addr(n->lhs);
        return;
    case ND_DEREF:
        gen_expr(n->lhs);
        load_val(n->ty);
        return;
    case ND_MEMBER:
        gen_addr(n);
        load_val(n->ty);
        return;
    case ND_ASSIGN:
        if (n->lhs->ty->kind == TY_STRUCT || n->lhs->ty->kind == TY_UNION) {
            gen_addr(n->lhs);
            emit_push_eax();
            gen_addr(n->rhs);
            gen_struct_copy(n->lhs->ty->size);
            return;
        }
        gen_addr(n->lhs);
        emit_push_eax();
        gen_expr(n->rhs);
        emit_pop_ecx();
        store_val(n->lhs->ty);
        return;
    case ND_ADD: case ND_SUB: case ND_MUL: case ND_DIV: case ND_MOD:
    case ND_BITAND: case ND_BITOR: case ND_BITXOR: case ND_SHL: case ND_SHR:
    case ND_EQ: case ND_NE: case ND_LT: case ND_LE:
        gen_binop(n);
        return;
    case ND_NEG:
        gen_expr(n->lhs); emit(0xF7); emit(0xD8);   /* neg eax */
        return;
    case ND_BITNOT:
        gen_expr(n->lhs); emit(0xF7); emit(0xD0);   /* not eax */
        return;
    case ND_NOT:
        gen_expr(n->lhs);
        emit(0x85); emit(0xC0);
        emit(0x0F); emit(0x94); emit(0xC0);
        emit(0x0F); emit(0xB6); emit(0xC0);
        return;
    case ND_LOGAND: {
        int lf = new_label(), le = new_label();
        gen_expr(n->lhs); emit_test_jz(lf);
        gen_expr(n->rhs); emit_test_jz(lf);
        emit_mov_eax_imm(1); emit_jmp(le);
        place_label(lf); emit(0x31); emit(0xC0);   /* xor eax,eax */
        place_label(le);
        return;
    }
    case ND_LOGOR: {
        int lt = new_label(), le = new_label();
        gen_expr(n->lhs); emit_test_jnz(lt);
        gen_expr(n->rhs); emit_test_jnz(lt);
        emit(0x31); emit(0xC0); emit_jmp(le);
        place_label(lt); emit_mov_eax_imm(1);
        place_label(le);
        return;
    }
    case ND_COND: {
        int lelse = new_label(), lend = new_label();
        gen_expr(n->cond); emit_test_jz(lelse);
        gen_expr(n->then); emit_jmp(lend);
        place_label(lelse); gen_expr(n->els);
        place_label(lend);
        return;
    }
    case ND_COMMA:
        gen_expr(n->lhs);
        gen_expr(n->rhs);
        return;
    case ND_CAST:
        gen_cast(n);
        return;
    case ND_STMT_EXPR:
        for (Node *c = n->body; c; c = c->next) gen_stmt(c);
        return;
    case ND_CALL: {
        /* collect args */
        int nargs = 0;
        for (Node *a = n->args; a; a = a->next) nargs++;
        Node **arr = xalloc((nargs + 1) * (int)sizeof(Node *));
        int i = 0;
        for (Node *a = n->args; a; a = a->next) arr[i++] = a;

        int is_direct = (n->fn->kind == ND_VAR && n->fn->var && n->fn->var->kind == OBJ_FUNC && !n->fn->str);

        /* builtin: __syscall0..__syscall3(num, ...) -> int $0x72 */
        if (is_direct && n->fn->var->name &&
            n->fn->var->name[0] == '_' && n->fn->var->name[1] == '_' &&
            xstrneq(n->fn->var->name, "__syscall", 9)) {
            int regs[4]; regs[0] = 0x58; regs[1] = 0x5B; regs[2] = 0x59; regs[3] = 0x5A;
            emit(0x53);                       /* push ebx */
            for (int k = 0; k < nargs; k++) { gen_expr(arr[k]); emit_push_eax(); }
            for (int k = nargs - 1; k >= 0; k--) emit(regs[k]);   /* pop into eax/ebx/ecx/edx */
            emit(0xCD); emit(0x72);           /* int 0x72 */
            emit(0x89); emit(0xC2);           /* mov edx,eax */
            emit(0x5B);                       /* pop ebx (restore) */
            emit(0x89); emit(0xD0);           /* mov eax,edx */
            return;
        }

        if (!is_direct) {
            gen_expr(n->fn);
            emit_push_eax();
        }
        for (int k = nargs - 1; k >= 0; k--) {
            gen_expr(arr[k]);
            emit_push_eax();
        }
        if (is_direct) {
            emit_call_obj(n->fn->var);
            if (nargs) { emit(0x81); emit(0xC4); emit4(nargs * 4); }   /* add esp,n*4 */
        } else {
            /* callee pointer is at [esp + nargs*4] */
            emit(0x8B); emit(0x84); emit(0x24); emit4(nargs * 4);      /* mov eax,[esp+n*4] */
            emit(0xFF); emit(0xD0);                                     /* call eax */
            emit(0x81); emit(0xC4); emit4((nargs + 1) * 4);             /* add esp,(n+1)*4 */
        }
        return;
    }
    }
    die_at(n->line, "codegen: cannot generate expression", 0);
}

void gen_stmt(Node *n) {
    if (!n) return;
    switch (n->kind) {
    case ND_BLOCK:
        for (Node *c = n->body; c; c = c->next) gen_stmt(c);
        return;
    case ND_EXPR_STMT:
        gen_expr(n->lhs);
        return;
    case ND_NULL:
        return;
    case ND_RETURN:
        if (n->lhs) {
            Node *v = n->lhs;
            if (g_gen_fn->ty->base && g_gen_fn->ty->base->kind != TY_VOID)
                v = new_cast(v, g_gen_fn->ty->base);
            gen_expr(v);
        }
        emit_jmp(g_epilogue);
        return;
    case ND_IF: {
        int lelse = new_label(), lend = new_label();
        gen_expr(n->cond); emit_test_jz(lelse);
        gen_stmt(n->then); emit_jmp(lend);
        place_label(lelse);
        if (n->els) gen_stmt(n->els);
        place_label(lend);
        return;
    }
    case ND_FOR: {
        int lbegin = new_label();
        if (n->init) gen_stmt(n->init);
        place_label(lbegin);
        if (n->cond) { gen_expr(n->cond); emit_test_jz(n->brk_label); }
        gen_stmt(n->then);
        place_label(n->cont_label);
        if (n->inc) gen_expr(n->inc);
        emit_jmp(lbegin);
        place_label(n->brk_label);
        return;
    }
    case ND_DO: {
        int lbegin = new_label();
        place_label(lbegin);
        gen_stmt(n->then);
        place_label(n->cont_label);
        gen_expr(n->cond);
        emit_test_jnz(lbegin);
        place_label(n->brk_label);
        return;
    }
    case ND_SWITCH: {
        gen_expr(n->cond);
        for (CaseLabel *c = n->cases; c; c = c->next) {
            emit(0x3D); emit4((int)c->val);           /* cmp eax,imm32 */
            emit_jcc(0x84, c->label);                  /* je case */
        }
        if (n->cont_label) emit_jmp(n->cont_label);    /* default */
        else emit_jmp(n->brk_label);
        gen_stmt(n->then);
        place_label(n->brk_label);
        return;
    }
    case ND_CASE:
        place_label(n->brk_label);
        gen_stmt(n->lhs);
        return;
    case ND_BREAK:
        emit_jmp(n->brk_label);
        return;
    case ND_CONTINUE:
        emit_jmp(n->cont_label);
        return;
    case ND_LABEL:
        place_label(named_label(n->label_name));
        gen_stmt(n->lhs);
        return;
    case ND_GOTO:
        emit_jmp(named_label(n->label_name));
        return;
    default:
        gen_expr(n);
        return;
    }
}

void gen_function(Obj *fn) {
    fn->code_off = g_code_len;
    g_gen_fn = fn;
    g_named = 0;
    g_epilogue = new_label();

    emit(0x55);                       /* push ebp */
    emit(0x89); emit(0xE5);           /* mov ebp,esp */
    emit(0x81); emit(0xEC); emit4(fn->stack_size + 16);   /* sub esp,frame */

    gen_stmt(fn->body);

    emit(0x31); emit(0xC0);           /* xor eax,eax  (default return) */
    place_label(g_epilogue);
    emit(0x89); emit(0xEC);           /* mov esp,ebp */
    emit(0x5D);                       /* pop ebp */
    emit(0xC3);                       /* ret */
}

/* -------- global variable initializers -------- */

void gv_write(unsigned char *buf, int off, long v, int size) {
    for (int i = 0; i < size; i++) buf[off + i] = (v >> (i * 8)) & 0xff;
}

/* returns 1 if e is an address constant; fills sym/str/addend */
int const_addr(Node *e, Obj **psym, Str **pstr, int *paddend) {
    *psym = 0; *pstr = 0; *paddend = 0;
    add_type(e);
    if (e->kind == ND_CAST) return const_addr(e->lhs, psym, pstr, paddend);
    if (e->kind == ND_VAR) {
        if (e->str) { *pstr = e->str; return 1; }
        if (e->var->kind == OBJ_FUNC) { *psym = e->var; return 1; }
        if (e->var->ty->kind == TY_ARRAY && !e->var->is_local) { *psym = e->var; return 1; }
        return 0;
    }
    if (e->kind == ND_ADDR) {
        Node *l = e->lhs;
        if (l->kind == ND_VAR && !l->var->is_local) { *psym = l->var; return 1; }
        if (l->kind == ND_DEREF) {
            /* &arr[k] */
            Node *add = l->lhs;
            if (add->kind == ND_ADD) {
                Obj *s; Str *st; int ad;
                if (const_addr(add->lhs, &s, &st, &ad)) {
                    long k = eval_const(add->rhs);
                    *psym = s; *pstr = st; *paddend = ad + (int)k;
                    return 1;
                }
            }
        }
        return 0;
    }
    if (e->kind == ND_ADD) {
        Obj *s; Str *st; int ad;
        if (const_addr(e->lhs, &s, &st, &ad)) {
            long k = eval_const(e->rhs);
            *psym = s; *pstr = st; *paddend = ad + (int)k;
            return 1;
        }
    }
    return 0;
}

void gv_init_scalar(Obj *owner, unsigned char *buf, int off, Type *ty) {
    Node *e = assign();
    add_type(e);
    if (ty->kind == TY_PTR) {
        Obj *s; Str *st; int ad;
        if (const_addr(e, &s, &st, &ad)) {
            if (st) add_data_fix(FIX_DATA_STR, owner, off, 0, st, ad);
            else add_data_fix(FIX_DATA_OBJ, owner, off, s, 0, ad);
            return;
        }
        gv_write(buf, off, eval_const(e), 4);
        return;
    }
    gv_write(buf, off, eval_const(e), ty->size);
}

void gv_init(Obj *owner, unsigned char *buf, int off, Type *ty);

void gv_init_aggregate(Obj *owner, unsigned char *buf, int off, Type *ty) {
    tk_expect("{");
    if (ty->kind == TY_ARRAY) {
        int esz = ty->base->size;
        int i = 0;
        while (!tk_is("}")) {
            if (i) tk_expect(",");
            if (tk_is("}")) break;
            gv_init(owner, buf, off + i * esz, ty->base);
            i++;
        }
        tk_expect("}");
        if (ty->is_incomplete) {
            ty->array_len = i;
            ty->size = i * esz;
        }
        return;
    }
    /* struct / union */
    Member *m = ty->members;
    int idx = 0;
    while (!tk_is("}")) {
        if (idx) tk_expect(",");
        if (tk_is("}")) break;
        if (!m) die("too many initializers for struct");
        gv_init(owner, buf, off + m->offset, m->ty);
        m = m->next;
        idx++;
    }
    tk_expect("}");
}

void gv_init(Obj *owner, unsigned char *buf, int off, Type *ty) {
    if (ty->kind == TY_ARRAY && ty->base->kind == TY_CHAR && TK->kind == T_STR) {
        char *s = TK->sval;
        int L = TK->slen + 1;
        tk_next();
        if (ty->is_incomplete) { ty->array_len = L; ty->size = L; }
        int lim = ty->array_len < L ? ty->array_len : L;
        for (int i = 0; i < lim; i++) buf[off + i] = (unsigned char)s[i];
        return;
    }
    if (tk_is("{")) { gv_init_aggregate(owner, buf, off, ty); return; }
    gv_init_scalar(owner, buf, off, ty);
}

void global_var(Obj *o, int has_init) {
    (void)has_init;
    /* Reserve worst-case space; resize after we know the final type size. */
    if (o->ty->is_incomplete && o->ty->kind == TY_ARRAY) {
        /* need a generous buffer; scan is hard, so cap at 64 KiB */
        o->init_data = xalloc(65536);
        o->data_off = 0;   /* real offset assigned in layout_data */
        gv_init(o, (unsigned char *)o->init_data, 0, o->ty);
        o->init_len = o->ty->size;
    } else {
        o->init_data = xalloc(o->ty->size ? o->ty->size : 1);
        o->data_off = 0;
        gv_init(o, (unsigned char *)o->init_data, 0, o->ty);
        o->init_len = o->ty->size;
    }
}

/* -------- data section layout -------- */

void data_reserve(int n) {
    if (g_data_len + n > g_data_cap) {
        int nc = g_data_cap ? g_data_cap * 2 : 4096;
        while (nc < g_data_len + n) nc *= 2;
        unsigned char *nn = xalloc(nc);
        for (int i = 0; i < g_data_len; i++) nn[i] = g_data[i];
        g_data = nn;
        g_data_cap = nc;
    }
}
void data_align(int a) {
    int na = align_to(g_data_len, a);
    data_reserve(na - g_data_len);
    while (g_data_len < na) g_data[g_data_len++] = 0;
}

void layout_data(void) {
    /* strings first */
    for (Str *s = g_strs; s; s = s->next) {
        s->data_off = g_data_len;
        data_reserve(s->len);
        for (int i = 0; i < s->len; i++) g_data[g_data_len++] = (unsigned char)s->data[i];
    }
    /* then globals with storage */
    for (Obj *o = g_globals; o; o = o->next) {
        if (o->kind != OBJ_VAR) continue;
        if (o->is_extern && !o->is_definition) continue;
        if (o->ty->size == 0) o->ty->size = 1;
        data_align(o->ty->align < 1 ? 1 : o->ty->align);
        int base = g_data_len;
        data_reserve(o->init_len ? o->init_len : o->ty->size);
        int nb = o->init_len ? o->init_len : o->ty->size;
        for (int i = 0; i < nb; i++)
            g_data[g_data_len++] = o->init_data ? (unsigned char)o->init_data[i] : 0;
        o->data_off = base;
    }
}

int obj_vaddr_off(Obj *o) {
    if (o->kind == OBJ_FUNC) return HDR_SIZE + o->code_off;
    return g_data_off + o->data_off;
}

void gen_text(void) {
    /* emit every defined function */
    for (Obj *o = g_globals; o; o = o->next) {
        if (o->kind == OBJ_FUNC && o->is_definition)
            gen_function(o);
    }
    layout_data();
}

/* -------- ELF image -------- */

void resolve_fixes(unsigned char *img) {
    for (Fix *f = g_fixes; f; f = f->next) {
        if (f->kind == FIX_LABEL_REL) {
            int tgt = g_label_off[f->t];
            patch4(g_code, f->loc, tgt - (f->loc + 4));
        } else if (f->kind == FIX_CALL) {
            if (!f->obj->is_definition)
                die_at(0, "undefined function referenced: ", f->obj->name);
            patch4(g_code, f->loc, f->obj->code_off - (f->loc + 4));
        } else if (f->kind == FIX_ABS_OBJ) {
            patch4(g_code, f->loc, LOAD_ADDR + obj_vaddr_off(f->obj) + f->addend);
        } else if (f->kind == FIX_ABS_STR) {
            patch4(g_code, f->loc, LOAD_ADDR + g_data_off + f->str->data_off + f->addend);
        } else if (f->kind == FIX_DATA_OBJ) {
            patch4(g_data, f->owner->data_off + f->loc,
                   LOAD_ADDR + obj_vaddr_off(f->obj) + f->addend);
        } else if (f->kind == FIX_DATA_STR) {
            patch4(g_data, f->owner->data_off + f->loc,
                   LOAD_ADDR + g_data_off + f->str->data_off + f->addend);
        }
    }
    (void)img;
}

void put4(unsigned char *b, int off, int v) {
    b[off] = v & 0xff; b[off+1] = (v>>8)&0xff; b[off+2] = (v>>16)&0xff; b[off+3] = (v>>24)&0xff;
}
void put2(unsigned char *b, int off, int v) {
    b[off] = v & 0xff; b[off+1] = (v>>8)&0xff;
}

int g_verbose;

void emit_elf(char *outpath) {
    Obj *mainfn = 0;
    int nf = 0, ng = 0, ns = 0;
    for (Obj *o = g_globals; o; o = o->next) {
        if (o->kind == OBJ_FUNC && o->is_definition) nf++;
        if (o->kind == OBJ_VAR && !(o->is_extern && !o->is_definition)) ng++;
        if (o->kind == OBJ_FUNC && o->is_definition && xstreq(o->name, "main")) mainfn = o;
    }
    for (Str *s = g_strs; s; s = s->next) ns++;
    if (g_verbose) {
        outs("cc: "); outd(nf); outs(" functions, "); outd(ng); outs(" globals, ");
        outd(ns); outs(" strings, "); outd(g_code_len); outs(" code, ");
        outd(g_data_len); outs(" data bytes\n");
    }
    if (!mainfn) die("no main() defined");

    g_data_off = align_to(HDR_SIZE + g_code_len, 16);
    int total = g_data_off + g_data_len;

    unsigned char *img = xalloc(total);
    xmemset((char *)img, 0, total);

    /* ELF header (52 bytes) */
    img[0] = 0x7f; img[1] = 'E'; img[2] = 'L'; img[3] = 'F';
    img[4] = 1;    /* EI_CLASS 32-bit */
    img[5] = 1;    /* EI_DATA LE */
    img[6] = 1;    /* EI_VERSION */
    put2(img, 16, 2);         /* e_type ET_EXEC */
    put2(img, 18, 3);         /* e_machine EM_386 */
    put4(img, 20, 1);         /* e_version */
    put4(img, 24, LOAD_ADDR + HDR_SIZE + mainfn->code_off);   /* e_entry */
    put4(img, 28, 52);        /* e_phoff */
    put4(img, 32, 0);         /* e_shoff */
    put4(img, 36, 0);         /* e_flags */
    put2(img, 40, 52);        /* e_ehsize */
    put2(img, 42, 32);        /* e_phentsize */
    put2(img, 44, 1);         /* e_phnum */
    put2(img, 46, 40);        /* e_shentsize */
    put2(img, 48, 0);         /* e_shnum */
    put2(img, 50, 0);         /* e_shstrndx */

    /* program header (32 bytes) at offset 52 */
    put4(img, 52, 1);            /* p_type PT_LOAD */
    put4(img, 56, 0);            /* p_offset */
    put4(img, 60, LOAD_ADDR);    /* p_vaddr */
    put4(img, 64, LOAD_ADDR);    /* p_paddr */
    put4(img, 68, total);        /* p_filesz */
    put4(img, 72, total);        /* p_memsz */
    put4(img, 76, 7);            /* p_flags RWX */
    put4(img, 80, 0x1000);       /* p_align */

    resolve_fixes(img);

    for (int i = 0; i < g_code_len; i++) img[HDR_SIZE + i] = g_code[i];
    for (int i = 0; i < g_data_len; i++) img[g_data_off + i] = g_data[i];

    if (sys_writefile(outpath, img, total) < 0) {
        outs("cc: cannot write "); outs(outpath); outs("\n");
        sys_exit(1);
    }
    outs("cc: wrote "); outs(outpath); outs(" ("); outd(total); outs(" bytes)\n");
}



/* ------------------------------------------------------------------ *
 *  driver
 * ------------------------------------------------------------------ */
int count_lines(char *s) {
    int n = 1;
    while (*s) { if (*s == '\n') n++; s++; }
    return n;
}

int main(int argc, char **argv) {
    char *inpath = 0;
    char *outpath = 0;
    char *rtpath = 0;
    int nostdlib = 0;

    for (int i = 1; i < argc; i++) {
        char *a = argv[i];
        if (xstreq(a, "-o")) { outpath = argv[++i]; }
        else if (xstreq(a, "--rt")) { rtpath = argv[++i]; }
        else if (xstreq(a, "-nostdlib")) { nostdlib = 1; }
        else if (xstreq(a, "-v") || xstreq(a, "--verbose")) { g_verbose = 1; }
        else if (a[0] == '-' && a[1] == 'D') { /* -Dxxx: no preprocessor, ignored */ }
        else if (a[0] == '-' && a[1] == 'I') { /* -Ixxx: no #include, ignored */ }
        else if (a[0] == '-' && a[1] == 'S') { /* ignored */ }
        else if (a[0] == '-' && a[1] == 'O') { /* -Ox: no optimiser, ignored */ }
        else if (a[0] == '-' && a[1]) { /* ignore other unknown flags */ }
        else inpath = a;
    }
    if (!inpath) die("usage: cc [-o out] [--rt prelude.c] [-nostdlib] input.c");

    int ulen = 0;
    char *usrc = sys_readfile(inpath, &ulen);
    if (!usrc) { outs("cc: cannot read "); outs(inpath); outs("\n"); sys_exit(1); }
    g_user_name = inpath;

    char *prelude = 0;
    int plen = 0;
    if (!nostdlib) {
        if (!rtpath) rtpath = "/hda/prelude.c";
        prelude = sys_readfile(rtpath, &plen);
        if (!prelude) {
            outs("cc: cannot read prelude "); outs(rtpath);
            outs(" (pass --rt PATH, or -nostdlib)\n");
            sys_exit(1);
        }
    }

    if (prelude) {
        /* one translation unit: [prelude]\n[user source]. line/file tracking
           resets at the boundary so diagnostics read <user>.c:N. */
        g_prelude_lines = count_lines(prelude) - 1;
        int total = plen + 1 + ulen + 1;
        char *buf = xalloc(total);
        xmemcpy(buf, prelude, plen);
        buf[plen] = '\n';
        xmemcpy(buf + plen + 1, usrc, ulen);
        buf[plen + 1 + ulen] = 0;
        g_src = buf;
    } else {
        g_src = usrc;
    }

    if (!outpath) {
        int n = xstrlen(inpath);
        char *o = xstrdup(inpath);
        if (n > 2 && o[n - 2] == '.' && o[n - 1] == 'c') o[n - 2] = 0;
        else { o = xalloc(n + 5); xmemcpy(o, inpath, n); xmemcpy(o + n, ".out", 5); }
        outpath = o;
    }
    g_outpath = outpath;

    init_types();
    TK = lex(g_src);
    enter_scope();           /* global scope */
    gen_program();           /* pass 1: parse+sema ; pass 2: codegen + ELF */
    return 0;
}
