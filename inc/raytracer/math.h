#pragma once

#define PI 3.14159265358979323846

typedef struct {
    float x, y, z;
} Vec3;

#define vec3(x, y, z) ((Vec3){(x), (y), (z)})

float vmag(Vec3 v);
Vec3 vsub(Vec3 a, Vec3 b);
Vec3 vadd(Vec3 a, Vec3 b);
Vec3 vmul(Vec3 v, float f);
Vec3 vnorm(Vec3 v);
float vdot(Vec3 a, Vec3 b);
Vec3 vproj(Vec3 a, Vec3 b);
Vec3 vrefl(Vec3 v, Vec3 n);
Vec3 vcross(Vec3 a, Vec3 b);

typedef struct {
    float x, y, z, w;
} Vec4;

#define vec4(x, y, z, w) ((Vec4){(x), (y), (z), (w)})

typedef struct {
    Vec4 rows[4];
} Matrix;

void matrixinit(Matrix *m);
Vec3 matrixmul(Matrix *m, Vec3 v);
void matrixtranslate(Matrix *m, Vec3 trans);
void matrixscale(Matrix *m, Vec3 scale);
void matrixrotate(Matrix *m, Vec3 axis, float degrees);
