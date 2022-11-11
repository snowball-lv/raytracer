#include <math.h>
#include <raytracer/obj.h>
#include <raytracer/raytracer.h>

float vmag(Vec3f v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3f vsub(Vec3f a, Vec3f b) {
    return (Vec3f){a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3f vadd(Vec3f a, Vec3f b) {
    return (Vec3f){a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3f vmul(Vec3f v, float f) {
    return (Vec3f){v.x * f, v.y * f, v.z * f};
}

Vec3f vnorm(Vec3f v) {
    return vmul(v, 1 / vmag(v));
}

float vdot(Vec3f a, Vec3f b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3f vproj(Vec3f a, Vec3f b) {
    return vmul(b, vdot(a, b) / (vmag(b) * vmag(b)));
}

// V has to point towards the intersection point
Vec3f vrefl(Vec3f v, Vec3f n) {
    return vsub(v, vmul(n, 2 * vdot(v, n)));
}

Vec3f vcross(Vec3f a, Vec3f b) {
    return (Vec3f){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}
