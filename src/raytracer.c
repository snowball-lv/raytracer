#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <raytracer/obj.h>
#include <raytracer/raytracer.h>

int testshape(Shape *s, Ray *r, Hit *h) {
    if (s->test) return s->test(s, r, h);
    return 0;
}

static int testsphere(Shape *s, Ray *r, Hit *h) {
    ShapeSphere *sp = (ShapeSphere *)s;
    Vec3 toc = vsub(sp->center, r->orig);
    float dot = vdot(toc, r->dir);
    int inside = vmag(toc) < sp->radius;
    if (!inside && dot < 0) return 0;
    Vec3 proj = vproj(toc, r->dir);
    Vec3 toray = vsub(proj, toc);
    float raydist = vmag(toray);
    if (raydist > sp->radius) return 0;
    float offset = sqrt(sp->radius * sp->radius - raydist * raydist);
    h->shape = s;
    if (inside) {
        if (dot > 0) h->dist = vmag(proj) + offset;
        else h->dist = offset - vmag(proj);
    }
    else
        h->dist = vmag(proj) - offset;
    h->point = vadd(r->orig, vmul(r->dir, h->dist));
    h->norm = vnorm(vsub(h->point, sp->center));
    return 1;
}

static int testplane(Shape *s, Ray *r, Hit *h) {
    ShapePlane *p = (ShapePlane *)s;
    if (vdot(r->dir, p->normal) > 0) return 0;
    Vec3f vp = vproj(r->dir, p->normal);
    Vec3f vpp = vproj(vsub(p->point, r->orig), p->normal);
    h->shape = s;
    h->point = vadd(r->orig, vmul(r->dir, vmag(vpp) / vmag(vp)));
    h->dist = vmag(vpp) / vmag(vp);
    h->norm = p->normal;
    return 1;
}

static int plane_intersect(Ray *r, Vec3f p, Vec3f n,
        Vec3f *ip, float *idist) {
    if (vdot(r->dir, n) > 0) return 0;
    Vec3f vp = vproj(r->dir, n);
    Vec3f vpp = vproj(vsub(p, r->orig), n);
    *ip = vadd(r->orig, vmul(r->dir, vmag(vpp) / vmag(vp)));
    *idist = vmag(vpp) / vmag(vp);
    return 1;
}

static float tri_area(Tri *tri) {
    Vec3f ab = vsub(tri->b, tri->a);
    Vec3f ac = vsub(tri->c, tri->a);
    return vmag(vcross(ab, ac)) / 2;
}

static int tri_intersect(Ray *r, Tri *tri,
        Vec3f *ip, Vec3f *in, float *idist) {
    Vec3f ab = vsub(tri->b, tri->a);
    Vec3f ac = vsub(tri->c, tri->a);
    // Vec3f cb = vsub(tri->b, tri->c);
    Vec3f tn = vnorm(vcross(ab, ac));
    Vec3f plane_ip;
    float plane_idist;
    if (!plane_intersect(r, tri->a, tn, &plane_ip, &plane_idist))
        return 0;
    *idist = plane_idist;
    *ip = plane_ip;
    *in = tn;
    float total = tri_area(tri);
    float x = tri_area(&(Tri){tri->a, tri->b, plane_ip}) / total;
    float y = tri_area(&(Tri){tri->a, tri->c, plane_ip}) / total;
    float z = tri_area(&(Tri){tri->b, tri->c, plane_ip}) / total;
    return x + y >= 0.0 && x + y <= 1.0
            && x + z >= 0.0 && x + z <= 1.0
            && y + z >= 0.0 && y + z <= 1.0;
}

static float max(float a, float b) {
    return a > b ? a : b;
}

static float maxdim(Obj *o) {
    float m = 0.0;
    for (int i = 0; i < o->nverts * 3; i++)
        m = max(m, fabs(o->verts[i]));
    return m;
}

static int testmesh(Shape *s, Ray *r, Hit *h) {
    ShapeMesh *m = (ShapeMesh *)s;
    Vec3 trans = vec3(1, 1, -3);
    Hit hit;
    m->bounds->center = trans;
    if (!testsphere(AS_SHAPE(m->bounds), r, &hit)) return 0;
    // *h = hit; return 1;
    float last_dist = FLT_MAX;
    int success = 0;
    for (int i = 0; i < m->obj->ntris; i++) {
        float *v0 = &m->obj->verts[m->obj->tris[i * 3 + 0] * 3];
        float *v1 = &m->obj->verts[m->obj->tris[i * 3 + 1] * 3];
        float *v2 = &m->obj->verts[m->obj->tris[i * 3 + 2] * 3];
        Tri tri = {
            vadd((Vec3f){v0[0], v0[1], v0[2]}, trans),
            vadd((Vec3f){v1[0], v1[1], v1[2]}, trans),
            vadd((Vec3f){v2[0], v2[1], v2[2]}, trans),
        };
        Vec3f ip, in;
        float idist;
        if (!tri_intersect(r, &tri, &ip, &in, &idist)) continue;
        if (idist > last_dist) continue;
        last_dist = idist;
        success = 1;
        h->shape = s;
        h->dist = idist;
        h->point = ip;
        h->norm = in;
    }
    return success;
}

static void *newshape(int type, int size) {
    Shape *s = malloc(size);
    memset(s, 0, size);
    s->type = type;
    return s;
}

ShapeSphere *newsphere(Vec3 center, float radius) {
    ShapeSphere *s = newshape(SHAPE_SPHERE, sizeof(ShapeSphere));
    s->center = center;
    s->radius = radius;
    s->shape.test = testsphere;
    return s;
}

ShapePlane *newplane(Vec3 point, Vec3 normal) {
    ShapePlane *p = newshape(SHAPE_PLANE, sizeof(ShapePlane));
    p->point = point;
    p->normal = vnorm(normal);
    p->shape.test = testplane;
    return p;
}

ShapeMesh *newmesh(Obj *obj) {
    ShapeMesh *m = newshape(SHAPE_MESH, sizeof(ShapePlane));
    m->obj = obj;
    m->bounds = newsphere(vec3(0, 0, 0), maxdim(obj));
    m->shape.test = testmesh;
    return m;
}

void freeshape(Shape *shape) {
    if (shape->type == SHAPE_MESH) {
        ShapeMesh *m = (ShapeMesh *)shape;
        freeshape(AS_SHAPE(m->bounds));
        freeobj(m->obj);
    }
    free(shape);
}
