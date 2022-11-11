#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <raytracer/util.h>
#include <raytracer/conf.h>

enum {
    T_NONE, T_EOF,
    T_L_BRACE, T_R_BRACE,
    T_L_BRACK, T_R_BRACK,
    T_COMMA, T_COLON,
    T_STR,
    T_NUM, T_DOT,
};

typedef struct {
    int type;
    char *str;
    int len;
} Tok;

typedef struct {
    char *src;
    Tok prev;
    Tok cur;
} Parser;

static Allocator _alloc = {"conf"};

static Tok _nexttok(Parser *p) {
    while (isspace(*p->src))
        p->src++;
    char *start = p->src;
    switch (*start) {
    case 0: return (Tok){T_EOF, start, 0};
    case '{': p->src++; return (Tok){T_L_BRACE, start, 1};
    case '}': p->src++; return (Tok){T_R_BRACE, start, 1};
    case '[': p->src++; return (Tok){T_L_BRACK, start, 1};
    case ']': p->src++; return (Tok){T_R_BRACK, start, 1};
    case ',': p->src++; return (Tok){T_COMMA, start, 1};
    case ':': p->src++; return (Tok){T_COLON, start, 1};
    case '"': p->src++; goto str;
    case '-': p->src++; goto num;
    }
    if (isdigit(*start)) goto num;
    err("conf: unexpected char: %c", *start);
    return (Tok){T_EOF};
str:
    while (*p->src) {
        p->src++;
        if (*(p->src - 1) == '"')
            return (Tok){T_STR, start + 1, p->src - start - 2};
    }
    err("conf: unterminated string");
num:
    while (isdigit(*p->src))
        p->src++;
    if (*p->src == '.') {
        p->src++;
        while (isdigit(*p->src))
            p->src++;
    }
    return (Tok){T_NUM, start, p->src - start};
}

static void advance(Parser *p) {
    p->prev = p->cur;
    p->cur = _nexttok(p);
}

static int match(Parser *p, int type) {
    if (p->cur.type != type) return 0;
    advance(p);
    return 1;
}

static void expect(Parser *p, int type) {
    if (match(p, type)) return;
    err("conf: expected %i, got %i", type, p->cur.type);
}

static ConfVal *newval(int type) {
    ConfVal *v = xmalloc(&_alloc, sizeof(ConfVal));
    memset(v, 0, sizeof(ConfVal));
    v->type = type;
    return v;
}

static ConfVal *newstr(Tok t) {
    char *str = xmalloc(&_alloc, t.len + 1);
    memcpy(str, t.str, t.len);
    str[t.len] = 0;
    ConfVal *v = newval(CONF_STR);
    v->as.str = str;
    return v;
}

static ConfVal *newnum(Tok t) {
    ConfVal *v = newval(CONF_NUM);
    v->as.num = atof(t.str);
    return v;
}

static ConfVal *newobj() {
    ConfVal *v = newval(CONF_OBJ);
    return v;
}

static ConfVal *newarr() {
    ConfVal *v = newval(CONF_ARR);
    return v;
}

static void pushkv(ConfObj *obj, Tok key, ConfVal *val) {
    obj->nkvs++;
    obj->keys = xrealloc(&_alloc, obj->keys, obj->nkvs * sizeof(ConfVal *));
    obj->vals = xrealloc(&_alloc, obj->vals, obj->nkvs * sizeof(ConfVal *));
    obj->keys[obj->nkvs - 1] = newstr(key);
    obj->vals[obj->nkvs - 1] = val;
}

static void pushval(ConfArr *arr, ConfVal *val) {
    arr->nvals++;
    arr->vals = xrealloc(&_alloc, arr->vals, arr->nvals * sizeof(ConfVal *));
    arr->vals[arr->nvals - 1] = val;
}

static ConfVal *parseexp(Parser *p);

static ConfVal *parseobj(Parser *p) {
    ConfVal *obj = newobj();
    expect(p, T_L_BRACE);
    while (!match(p, T_R_BRACE)) {
        expect(p, T_STR);
        Tok key = p->prev;
        expect(p, T_COLON);
        ConfVal *val = parseexp(p);
        match(p, T_COMMA);
        pushkv(&obj->as.obj, key, val);
    }
    return obj;
}

static ConfVal *parsearray(Parser *p) {
    ConfVal *arr = newarr();
    expect(p, T_L_BRACK);
    while (!match(p, T_R_BRACK)) {
        ConfVal *val = parseexp(p);
        match(p, T_COMMA);
        pushval(&arr->as.arr, val);
    }
    return arr;
}

ConfVal *parseexp(Parser *p) {
    switch (p->cur.type) {
    case T_L_BRACE: return parseobj(p);
    case T_L_BRACK: return parsearray(p);
    case T_STR: expect(p, T_STR); return newstr(p->prev);
    case T_NUM: expect(p, T_NUM); return newnum(p->prev);
    }
    err("conf: unexpected tok %i", p->cur.type);
    return 0;
}

Conf *parseconf(const char *file) {
    Conf *conf = xmalloc(&_alloc, sizeof(Conf));
    memset(conf, 0, sizeof(Conf));
    char *src = readfile(file);
    Parser p = {0};
    p.src = src;
    advance(&p);
    conf->root = parseexp(&p);
    free(src);
    return conf;
}

static void freeval(ConfVal *v) {
    switch (v->type) {
    case CONF_STR:
        xfree(v->as.str);
        break;
    case CONF_NUM:
        break;
    case CONF_ARR:
        for (int i = 0; i < v->as.arr.nvals; i++)
            freeval(v->as.arr.vals[i]);
        if (v->as.arr.nvals)
            xfree(v->as.arr.vals);
        break;
    case CONF_OBJ:
        for (int i = 0; i < v->as.obj.nkvs; i++) {
            freeval(v->as.obj.keys[i]);
            freeval(v->as.obj.vals[i]);
        }
        if (v->as.obj.nkvs) {
            xfree(v->as.obj.keys);
            xfree(v->as.obj.vals);
        }
        break;
    }
    xfree(v);
}

void freeconf(Conf *conf) {
    freeval(conf->root);
    xfree(conf);
}

static void dumpval(ConfVal *v, int indent) {
    switch (v->type) {
    case CONF_STR: printf("%s", v->as.str); break;
    case CONF_NUM: printf("%.2f", v->as.num); break;
    case CONF_ARR:
        printf("[ ");
        for (int i = 0; i < v->as.arr.nvals; i++) {
            ConfVal *val = v->as.arr.vals[i];
            dumpval(val, indent + 4);
            printf(" ");
        }
        printf("]");
        break;
    case CONF_OBJ:
        printf("{\n");
        for (int i = 0; i < v->as.obj.nkvs; i++) {
            ConfVal *key = v->as.obj.keys[i];
            ConfVal *val = v->as.obj.vals[i];
            printf("%*s", indent + 2, "");
            dumpval(key, 0); // always str
            printf(": ");
            dumpval(val, indent + 2);
            printf("\n");
        }
        printf("%*s}", indent, "");
        break;
    }
}

void dumpconf(Conf *conf) {
    printf("--- conf dump ---\n");
    dumpval(conf->root, 0);
    printf("\n");
}

void dumpconfmem() {
    printf("conf mem usage: %u bytes\n", _alloc.size);
}

ConfVal *confobjget(ConfVal *obj, const char *name) {
    if (obj->type != CONF_OBJ) return 0;
    ConfObj *_obj = &obj->as.obj;
    for (int i = 0; i < _obj->nkvs; i++) {
        ConfVal *key = _obj->keys[i];
        if (key->type != CONF_STR) continue;
        if (strcmp(key->as.str, name) == 0)
            return _obj->vals[i];
    }
    return 0;
}

ConfVal *confobjgettype(ConfVal *obj, const char *name, int type) {
    ConfVal *val = confobjget(obj, name);
    if (val && val->type == type) return val;
    return 0;
}

float confobjgetnum(ConfVal *obj, const char *name, float def) {
    ConfVal *val = confobjgettype(obj, name, CONF_NUM);
    if (!val) return def;
    return val->as.num;
}

ConfVal *confobjgetarr(ConfVal *obj, const char *name) {
    return confobjgettype(obj, name, CONF_ARR);
}

const char *confobjgetstr(ConfVal *obj, const char *name, const char *def) {
    ConfVal *val = confobjgettype(obj, name, CONF_STR);
    if (!val) return def;
    return val->as.str;
}

int confarrsize(ConfVal *arr) {
    if (arr && arr->type == CONF_ARR) return arr->as.arr.nvals;
    return 0;
}

ConfVal *confarrget(ConfVal *arr, int i) {
    if (!arr || arr->type != CONF_ARR) return 0;
    if (i < 0 || i >= arr->as.arr.nvals) return 0;
    return arr->as.arr.vals[i];
}

static ConfVal *confarrgettype(ConfVal *arr, int i, int type) {
    if (!arr || arr->type != CONF_ARR) return 0;
    if (i < 0 || i >= arr->as.arr.nvals) return 0;
    ConfVal *val = arr->as.arr.vals[i];
    if (val->type != type) return 0;
    return val;
}

float confarrgetnum(ConfVal *obj, int i, float def) {
    ConfVal *val = confarrgettype(obj, i, CONF_NUM);
    if (!val) return def;
    return val->as.num;
}
