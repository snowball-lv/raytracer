#pragma once

#define AS_SHAPE(ptr) (&(ptr)->shape)

typedef struct Shape Shape;
typedef struct Scene Scene;

typedef struct {
    Vec3 a, b, c;
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

typedef struct {
    Vec3 diffuse;
    float reflectiveness;
} Material;

struct Shape {
    enum {
        SHAPE_NONE,
        SHAPE_SPHERE,
        SHAPE_PLANE,
        SHAPE_MESH,
    } type;
    Material mat;
    Matrix transform;
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

void shapetranslate(Shape *s, Vec3 trans);
void shaperotate(Shape *s, Vec3 axis, float degrees);
void shapescale(Shape *s, Vec3 scale);

typedef struct {
    unsigned char r, g, b;
} Color;

typedef struct {
    Vec3 pos;
    float intensity;
} Light;

struct Scene {
    const char *output;
    int width;
    int height;
    float vfov;
    float aspect;
    Light **lights;
    int nlights;
    Vec3 background;
    float ambiance;
    Shape **shapes;
    int nshapes;
};

Scene *newscene(const char *file);
void freescene(Scene *s);
void addshape(Scene *s, Shape *shape);
