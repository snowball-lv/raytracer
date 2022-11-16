#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <raytracer/math.h>
#include <raytracer/util.h>
#include <raytracer/obj.h>
#include <raytracer/raytracer.h>

enum {
    T_NONE, T_EOF,
    T_ID, T_V, T_VT, T_VN,
    T_F,
    T_S,
    T_FLOAT, T_INT,
    T_SLASH,
    T_USEMTL,
};

typedef struct {
    int type;
    char *str;
    int len;
} Tok;

typedef struct {
    char *src;
    Obj *obj;
    Tok cur;
    Tok prev;
} Parser;

static Tok _nexttok(Parser *p) {
    while (isspace(*p->src))
        p->src++;
    char *start = p->src;
    switch (*start) {
    case 0: return (Tok){T_EOF};
    case '#':
        while (*p->src && *p->src != '\n')
            p->src++;
        return _nexttok(p);
    case '-': p->src++; goto num;
    case '/': p->src++; return (Tok){T_SLASH, start, 1};
    }
    if (isalpha(*p->src)) goto id;
    else if (isdigit(*p->src)) goto num;
    err("unexpected char: %c", *start);
    return (Tok){T_EOF};
num:
    while (isdigit(*p->src))
        p->src++;
    if (*p->src != '.')
        return (Tok){T_INT, start, p->src - start};
    p->src++;
    while (isdigit(*p->src))
        p->src++;
    return (Tok){T_FLOAT, start, p->src - start};
id:
    while (*p->src && !isspace(*p->src))
        p->src++;
    return (Tok){T_ID, start, p->src - start};
}

static int tokstreq(Tok *t, const char *str) {
    int len = strlen(str);
    if (t->len != len) return 0;
    return strncmp(t->str, str, len) == 0;
}

static Tok nexttok(Parser *p) {
    Tok t = _nexttok(p);
    if (t.type == T_ID) {
        if (tokstreq(&t, "v")) t.type = T_V;
        else if (tokstreq(&t, "vt")) t.type = T_VT;
        else if (tokstreq(&t, "vn")) t.type = T_VN;
        else if (tokstreq(&t, "f")) t.type = T_F;
        else if (tokstreq(&t, "usemtl")) t.type = T_USEMTL;
        else if (tokstreq(&t, "s")) t.type = T_S;
    }
    return t;
}
static void advance(Parser *p) {
    p->prev = p->cur;
    p->cur = nexttok(p);
}

static int match(Parser *p, int type) {
    if (p->cur.type != type) return 0;
    advance(p);
    return 1;
}

static void expect(Parser *p, int type) {
    if (match(p, type)) return;
    err("obj: expected %i, got %i", type, p->cur.type);
}

static void pushvert(Obj *obj, float *v) {
    obj->nverts++;
    obj->verts = realloc(obj->verts, obj->nverts * 3 * sizeof(float));
    memcpy(&obj->verts[obj->nverts * 3 - 3], v, 3 * sizeof(float));
}

static void pushtri(Obj *obj, int *tri) {
    obj->ntris++;
    obj->tris = realloc(obj->tris, obj->ntris * 3 * sizeof(int));
    memcpy(&obj->tris[obj->ntris * 3 - 3], tri, 3 * sizeof(int));
}

static void parse(Parser *p) {
    while (!match(p, T_EOF)) {
        if (match(p, T_V)) {
            float v[3];
            for (int i = 0; i < 3; i++) {
                expect(p, T_FLOAT);
                v[i] = atof(p->prev.str);
            }
            pushvert(p->obj, v);
            continue;
        }
        else if (match(p, T_VT)) {
            for (int i = 0; i < 2; i++)
                expect(p, T_FLOAT);
            continue;
        }
        else if (match(p, T_VN)) {
            for (int i = 0; i < 3; i++)
                expect(p, T_FLOAT);
            continue;
        }
        else if (match(p, T_F)) {
            int tri[3];
            for (int i = 0; i < 3; i++) {
                expect(p, T_INT);
                tri[i] = atoi(p->prev.str) - 1;
                // tex
                if (match(p, T_SLASH))
                    match(p, T_INT);
                // norm
                if (match(p, T_SLASH))
                    match(p, T_INT);
            }
            pushtri(p->obj, tri);
            continue;
        }
        else if (match(p, T_USEMTL)) {
            expect(p, T_ID);
            continue;
        }
        else if (match(p, T_S)) {
            expect(p, T_INT);
            continue;
        }
        err("unexpected tok: %i, %.*s", p->cur.type, p->cur.len, p->cur.str);
    }
}

Obj *newobj(const char *file) {
    Parser p;
    char *full = readfile(file);
    p.src = full;
    p.obj = malloc(sizeof(Obj));
    memset(p.obj, 0, sizeof(Obj));
    advance(&p);
    parse(&p);
    free(full);
    return p.obj;
}

void freeobj(Obj *o) {
    if (o->verts) free(o->verts);
    if (o->tris) free(o->tris);
    free(o);
}
