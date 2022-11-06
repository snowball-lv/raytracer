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
