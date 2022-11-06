#pragma once

typedef struct {
    float *verts;
    int nverts;
    int *tris;
    int ntris;
} Obj;

Obj *newobj(const char *file);
void freeobj(Obj *o);
