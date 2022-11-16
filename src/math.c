#include <math.h>
#include <raytracer/math.h>

float vmag(Vec3 v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 vsub(Vec3 a, Vec3 b) {
    return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 vadd(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 vmul(Vec3 v, float f) {
    return (Vec3){v.x * f, v.y * f, v.z * f};
}

Vec3 vnorm(Vec3 v) {
    return vmul(v, 1 / vmag(v));
}

float vdot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 vproj(Vec3 a, Vec3 b) {
    return vmul(b, vdot(a, b) / (vmag(b) * vmag(b)));
}

// V has to point towards the intersection point
Vec3 vrefl(Vec3 v, Vec3 n) {
    return vsub(v, vmul(n, 2 * vdot(v, n)));
}

Vec3 vcross(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

void matrixinit(Matrix *m) {
    m->rows[0] = vec4(1, 0, 0, 0);
    m->rows[1] = vec4(0, 1, 0, 0);
    m->rows[2] = vec4(0, 0, 1, 0);
    m->rows[3] = vec4(0, 0, 0, 1);
}

static float v4dot(Vec4 a, Vec4 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

Vec3 matrixmul(Matrix *m, Vec3 v) {
    Vec4 v4 = vec4(v.x, v.y, v.z, 1);
    float x = v4dot(m->rows[0], v4);
    float y = v4dot(m->rows[1], v4);
    float z = v4dot(m->rows[2], v4);
    return vec3(x, y, z);
}

void matrixtranslate(Matrix *m, Vec3 trans) {
    m->rows[0].w += trans.x;
    m->rows[1].w += trans.y;
    m->rows[2].w += trans.z;
}

void matrixscale(Matrix *m, Vec3 scale) {
    m->rows[0].x *= scale.x;
    m->rows[1].y *= scale.y;
    m->rows[2].z *= scale.z;
}

void matrixrotate(Matrix *m, Vec3 axis, float degrees) {
    float radians = degrees * PI / 180.0;
    float c = cos(radians);
    float s = sin(radians);
    Vec3 n = vnorm(axis);
    Matrix r = {
        vec4(n.x * n.x * (1 - c) + c, n.x * n.y * (1 - c) - n.z * s, n.x * n.z * (1 - c) + n.y * s, 0),
        vec4(n.y * n.x * (1 - c) + n.z * s, n.y * n.y * (1 - c) + c, n.y * n.z * (1 - c) - n.x * s, 0),
        vec4(n.z * n.x * (1 - c) - n.y * s, n.z * n.y * (1 - c) + n.x * s, n.z * n.z * (1 - c) + c, 0),
        vec4(0, 0, 0, 1),
    };
    Matrix t = *m;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            ((float *)&m->rows[i])[j] = v4dot(t.rows[i], r.rows[j]);
        }
    }
}
