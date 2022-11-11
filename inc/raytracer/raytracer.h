#pragma once

typedef struct {
    float x, y, z;
} Vec3f;

float vmag(Vec3f v);
Vec3f vsub(Vec3f a, Vec3f b);
Vec3f vadd(Vec3f a, Vec3f b);
Vec3f vmul(Vec3f v, float f);
Vec3f vnorm(Vec3f v);
float vdot(Vec3f a, Vec3f b);
Vec3f vproj(Vec3f a, Vec3f b);
Vec3f vrefl(Vec3f v, Vec3f n);
Vec3f vcross(Vec3f a, Vec3f b);

typedef Vec3f Vec3;
#define vec3(x, y, z) ((Vec3){(x), (y), (z)})
#define AS_SHAPE(ptr) (&(ptr)->shape)

typedef struct Shape Shape;
typedef struct Scene Scene;

typedef struct {
    Vec3f a, b, c;
} Tri;

typedef struct {
    Vec3 orig;
    Vec3 dir;
} Ray;

typedef struct {
    Shape *shape;
    float dist;
    Vec3 point;
    Vec3 norm;
} Hit;

struct Shape {
    enum {
        SHAPE_NONE,
        SHAPE_SPHERE,
        SHAPE_PLANE,
        SHAPE_MESH,
    } type;
    int (*test)(Shape *s, Ray *r, Hit *h);
};

typedef struct {
    Shape shape;
    Vec3 center;
    float radius;
} ShapeSphere;

typedef struct {
    Shape shape;
    Vec3 point;
    Vec3 normal;
} ShapePlane;

typedef struct {
    Shape shape;
    Obj *obj;
    ShapeSphere *bounds;
} ShapeMesh;

ShapeSphere *newsphere(Vec3 center, float radius);
ShapePlane *newplane(Vec3 point, Vec3 normal);
ShapeMesh *newmesh(Obj *obj);
void freeshape(Shape *shape);
int testshape(Shape *s, Ray *r, Hit *h);
