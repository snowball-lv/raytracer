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
    ConfVal *v = malloc(sizeof(ConfVal));
    memset(v, 0, sizeof(ConfVal));
    v->type = type;
    return v;
}

static ConfVal *newstr(Tok t) {
    char *str = malloc(t.len + 1);
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
    obj->keys = realloc(obj->keys, obj->nkvs * sizeof(ConfVal *));
    obj->vals = realloc(obj->vals, obj->nkvs * sizeof(ConfVal *));
    obj->keys[obj->nkvs - 1] = newstr(key);
    obj->vals[obj->nkvs - 1] = val;
}

static void pushval(ConfArr *arr, ConfVal *val) {
    arr->nvals++;
    arr->vals = realloc(arr->vals, arr->nvals * sizeof(ConfVal *));
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
    Conf *conf = malloc(sizeof(Conf));
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
        free(v->as.str);
        break;
    case CONF_NUM:
        break;
    case CONF_ARR:
        for (int i = 0; i < v->as.arr.nvals; i++)
            freeval(v->as.arr.vals[i]);
        free(v->as.arr.vals);
        break;
    case CONF_OBJ:
        for (int i = 0; i < v->as.obj.nkvs; i++) {
            freeval(v->as.obj.keys[i]);
            freeval(v->as.obj.vals[i]);
        }
        free(v->as.obj.keys);
        free(v->as.obj.vals);
        break;
    }
    free(v);
}

void freeconf(Conf *conf) {
    freeval(conf->root);
    free(conf);
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
