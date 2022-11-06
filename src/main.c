#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <raytracer/raytracer.h>
#include <raytracer/teapot.h>
#include <raytracer/obj.h>
#include <raytracer/conf.h>

#define PI 3.14159265358979323846
#define MAX_RECUR 3

typedef struct {
    unsigned char r, g, b;
} Color;

#define RED (Color){255}
#define GREEN (Color){0, 255}
#define BLUE (Color){0, 0, 255}
#define MAGENTA (Color){255, 0, 255}
#define CYAN (Color){0, 255, 255}
#define YELLOW (Color){255, 255, 0}

typedef struct {
    int width;
    int height;
    Color *pixels;
} Bitmap;

static void initbitmap(Bitmap *bmp, int w, int h) {
    bmp->width = w;
    bmp->height = h;
    bmp->pixels = malloc(w * h * sizeof(Color));
}

static void freebitmap(Bitmap *bmp) {
    free(bmp->pixels);
}

static void clear(Bitmap *bmp, Color c) {
    for (int y = 0; y < bmp->height; y++)
        for (int x = 0; x < bmp->width; x++)
            bmp->pixels[y * bmp->width + x] = c;
}

static void output(Bitmap *bmp, char *file) {
    FILE *f = fopen(file, "w");
    fprintf(f, "P6\n%i %i\n%i\n", bmp->width, bmp->height, 255);
    for (int y = 0; y < bmp->height; y++) {
        for (int x = 0; x < bmp->width; x++) {
            Color c = bmp->pixels[y * bmp->width + x];
            fwrite(&c, 3, 1, f);
        }
    }
    fclose(f);
}

static float clamp(float f, float min, float max) {
    return f < min ? min : (f > max ? max : f);
}

static float clampi(int i, int min, int max) {
    return i < min ? min : (i > max ? max : i);
}

static Color colormul(Color c, float f) {
    // f = clamp(f, 0.0, 1.0);
    return (Color) {
        .r = clampi(c.r * f, 0, 255),
        .g = clampi(c.g * f, 0, 255),
        .b = clampi(c.b * f, 0, 255),
    };
}

static Color coloradd(Color a, Color b) {
    return (Color) {
        clampi(a.r + b.r, 0, 255),
        clampi(a.g + b.g, 0, 255),
        clampi(a.b + b.b, 0, 255),
    };
}

typedef struct {
    Color color;
} Material;

typedef struct {
    Vec3f pos;
    float radius;
    Material m;
} Sphere;

typedef struct {
    Vec3f orig;
    Vec3f dir;
} Ray;

typedef struct {
    Vec3f pos;
    float intensity;
} Light;

typedef struct {
    Vec3f a, b, c;
} Tri;

typedef struct {
    Tri *tris;
    int ntris;
    Vec3f pos;
    float radius;
} Mesh;

static Mesh TEAPOT;

typedef struct {
    float vfov;
    float aspect;
    Sphere *spheres;
    int nspheres;
    Light *lights;
    int nlights;
    Color background;
    float ambient;
    Tri *tris;
    int ntris;
} Scene;

static Obj *suzanne;

static int sphere_intersect(Sphere *s, Ray *r, float *dist) {
    Vec3f toc = vsub(s->pos, r->orig);
    float dot = vdot(toc, r->dir);
    int inside = vmag(toc) < s->radius;
    if (!inside && dot < 0) return 0;
    Vec3f proj = vproj(toc, r->dir);
    Vec3f toray = vsub(proj, toc);
    float raydist = vmag(toray);
    if (raydist > s->radius) return 0;
    float offset = sqrt(s->radius * s->radius - raydist * raydist);
    if (inside) {
        if (dot > 0) *dist = vmag(proj) + offset;
        else *dist = offset - vmag(proj);
    }
    else
        *dist = vmag(proj) - offset;
    return 1;
}

static float torad(float deg) {
    return deg * PI / 180.0;
}

static int intersect(Ray *r, Scene *s, Vec3f *point, Vec3f *n, float *dist) {
    *dist = FLT_MAX;
    int hit = 0;
    for (int i = 0; i < s->nspheres; i++) {
        Sphere *sp = &s->spheres[i];
        float sp_dist;
        if (sphere_intersect(sp, r, &sp_dist)) {
            if (sp_dist > *dist) continue;
            *point = vadd(r->orig, vmul(r->dir, sp_dist));
            *n = vnorm(vsub(*point, sp->pos));
            *dist = sp_dist;
            hit = 1;
        }
    }
    return hit;
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
    Vec3f cb = vsub(tri->b, tri->c);
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

static Color cast(Ray *r, Scene *s, int recur, int *hit) {
    Color c = s->background;
    float last_dist = FLT_MAX;
    if (hit) *hit = 0;
    if (recur < MAX_RECUR) {
        Vec3f p = {-1, -1.5, 0};
        Vec3f n = {0.5, 1, 0};
        n = vnorm(n);
        Vec3f ip;
        float idist;
        if (plane_intersect(r, p, n, &ip, &idist)) {
            last_dist = idist;
            if (hit) *hit = 1;
            c = colormul(s->background, 0.8);
            // lights
            float intensity = s->ambient;
            for (int k = 0; k < s->nlights; k++) {
                Light *light = &s->lights[k];
                Vec3f l = vsub(light->pos, ip);
                // occlusion
                {
                    Ray ray = {ip, vnorm(l)};
                    ray.orig = vadd(ray.orig, vmul(ray.dir, 0.0001));
                    Vec3f ipoint, in;
                    float idist;
                    if (intersect(&ray, s, &ipoint, &in, &idist))
                        if (idist < vmag(l))
                            continue;
                }
                Vec3f rv = vrefl(vsub(ip, light->pos), n);
                float light_i = vdot(vnorm(l), n) * light->intensity;
                float refl = vdot(vnorm(vsub(r->orig, ip)), vnorm(rv));
                refl = clamp(refl, 0.0, 1.0);
                light_i += pow(refl, 16);
                intensity += clamp(light_i, 0.0, FLT_MAX);
            }
            // reflection
            if (recur < MAX_RECUR) {
                Vec3f refl = vrefl(r->dir, n);
                Ray ray = {ip, vnorm(refl)};
                int rhit;
                Color rc = cast(&ray, s, recur + 1, &rhit);
                if (rhit) {
                    c = coloradd(colormul(c, 0.8), colormul(rc, 0.5));
                }
            }
            c = colormul(c, intensity);
        }
    }
    for (int i = 0; i < s->nspheres; i++) {
        Sphere *sp = &s->spheres[i];
        float dist;
        if (sphere_intersect(sp, r, &dist)) {
            if (dist > last_dist) continue;
            if (hit) *hit = 1;
            last_dist = dist;
            Vec3f point = vadd(r->orig, vmul(r->dir, dist));
            Vec3f n = vnorm(vsub(point, sp->pos));
            Vec3f v = vsub(r->orig, point);
            Vec3f vr = vrefl(vsub(point, r->orig), n);
            c = sp->m.color;
            // lights
            float intensity = s->ambient;
            for (int k = 0; k < s->nlights; k++) {
                Light *light = &s->lights[k];
                Vec3f l = vsub(light->pos, point);
                // occlusion
                {
                    Ray ray = {point, vnorm(l)};
                    ray.orig = vadd(ray.orig, vmul(ray.dir, 0.0001));
                    Vec3f ipoint, in;
                    float idist;
                    if (intersect(&ray, s, &ipoint, &in, &idist))
                        if (idist < vmag(l))
                            continue;
                }
                Vec3f r = vrefl(vsub(point, light->pos), n);
                float light_i = vdot(vnorm(l), n) * light->intensity;
                float refl = vdot(vnorm(v), vnorm(r));
                refl = clamp(refl, 0.0, 1.0);
                light_i += pow(refl, 16);
                intensity += clamp(light_i, 0.0, FLT_MAX);
            }
            // reflections
            if (recur < MAX_RECUR) {
                Ray ray = {point, vnorm(vr)};
                ray.orig = vadd(ray.orig, vmul(n, 0.0001));
                int hit;
                Color rc = cast(&ray, s, recur + 1, &hit);
                if (hit)
                    c = coloradd(c, colormul(rc, 0.5));
            }
            c = colormul(c, intensity);
        }
    }
    // for (int i = 0; i < s->ntris; i++) {
    //     Tri *tri = &s->tris[i];
    //     Vec3f ip, in;
    //     float idist;
    //     if (!tri_intersect(r, tri, &ip, &in, &idist)) continue;
    //     if (idist > last_dist) continue;
    //     last_dist = idist;
    //     if (hit) *hit = 1;
    //     c = MAGENTA;
    // }
    {
        Vec3f pos = {1, 1, -3};
        Sphere ss = {pos, maxdim(suzanne)};
        float idist;
        if (sphere_intersect(&ss, r, &idist)) {
            int intersected = 0;
            Color lc;
            for (int i = 0; i < suzanne->ntris; i++) {
                float *v0 = &suzanne->verts[suzanne->tris[i * 3 + 0] * 3];
                float *v1 = &suzanne->verts[suzanne->tris[i * 3 + 1] * 3];
                float *v2 = &suzanne->verts[suzanne->tris[i * 3 + 2] * 3];
                Tri tri = {
                    vadd((Vec3f){v0[0], v0[1], v0[2]}, pos),
                    vadd((Vec3f){v1[0], v1[1], v1[2]}, pos),
                    vadd((Vec3f){v2[0], v2[1], v2[2]}, pos),
                };
                Vec3f ip, in;
                float idist;
                if (!tri_intersect(r, &tri, &ip, &in, &idist)) continue;
                if (idist > last_dist) continue;
                last_dist = idist;
                if (hit) *hit = 1;
                intersected = 1;
                lc = colormul(MAGENTA, 0.5);

                // lights
                float intensity = s->ambient;
                for (int k = 0; k < s->nlights; k++) {
                    Light *light = &s->lights[k];
                    Vec3f l = vsub(light->pos, ip);
                    // occlusion
                    {
                        Ray ray = {ip, vnorm(l)};
                        ray.orig = vadd(ray.orig, vmul(ray.dir, 0.0001));
                        Vec3f ipoint, in;
                        float idist;
                        if (intersect(&ray, s, &ipoint, &in, &idist))
                            if (idist < vmag(l))
                                continue;
                    }
                    Vec3f rv = vrefl(vsub(ip, light->pos), in);
                    float light_i = vdot(vnorm(l), in) * light->intensity;
                    float refl = vdot(vnorm(vsub(r->orig, ip)), vnorm(rv));
                    refl = clamp(refl, 0.0, 1.0);
                    light_i += pow(refl, 16);
                    intensity += clamp(light_i, 0.0, FLT_MAX);
                }

                // reflection
                if (recur < MAX_RECUR) {
                    Vec3f refl = vrefl(r->dir, in);
                    Ray ray = {ip, vnorm(refl)};
                    int rhit;
                    Color rc = cast(&ray, s, recur + 1, &rhit);
                    if (rhit) {
                        lc = coloradd(colormul(lc, 0.8), colormul(rc, 0.5));
                    }
                }

                lc = colormul(lc, intensity);
            }
            if (intersected)
                c = lc;
        }
    }
    Sphere ts = {TEAPOT.pos, TEAPOT.radius};
    float idist;
    if (sphere_intersect(&ts, r, &idist) && 0) {
        for (int i = 0; i < TEAPOT.ntris; i++) {

            Tri *tri = &TEAPOT.tris[i];
            Vec3f ip, in;
            float idist;
            if (!tri_intersect(r, tri, &ip, &in, &idist)) continue;
            if (idist > last_dist) continue;
            last_dist = idist;
            if (hit) *hit = 1;
            c = colormul(MAGENTA, 0.5);
            
            // lights
            float intensity = s->ambient;
            for (int k = 0; k < s->nlights; k++) {
                Light *light = &s->lights[k];
                Vec3f l = vsub(light->pos, ip);
                // occlusion
                {
                    Ray ray = {ip, vnorm(l)};
                    ray.orig = vadd(ray.orig, vmul(ray.dir, 0.0001));
                    Vec3f ipoint, in;
                    float idist;
                    if (intersect(&ray, s, &ipoint, &in, &idist))
                        if (idist < vmag(l))
                            continue;
                }
                Vec3f rv = vrefl(vsub(ip, light->pos), in);
                float light_i = vdot(vnorm(l), in) * light->intensity;
                float refl = vdot(vnorm(vsub(r->orig, ip)), vnorm(rv));
                refl = clamp(refl, 0.0, 1.0);
                light_i += pow(refl, 16);
                intensity += clamp(light_i, 0.0, FLT_MAX);
            }

            // reflection
            if (recur < MAX_RECUR) {
                Vec3f refl = vrefl(r->dir, in);
                Ray ray = {ip, vnorm(refl)};
                int rhit;
                Color rc = cast(&ray, s, recur + 1, &rhit);
                if (rhit) {
                    c = coloradd(colormul(c, 0.8), colormul(rc, 0.5));
                }
            }

            c = colormul(c, intensity);
        }
    }
    return c;
}

static void render(Bitmap *bmp) {
    Sphere spheres[] = {
        {.pos = {1.0, 0, -5}, .radius = 1, .m = {.color = {150, 100, 50}}},
        {.pos = {-1, 0, -3}, .radius = 1, .m = {.color = {100, 150, 50}}},
        {.pos = {1.5, -1.0, -6}, .radius = 1, .m = {.color = {100, 50, 150}}},
    };
    Light lights[] = {
        {{-2, 2, 0}, 1.0},
        // {{0, 0, 0}, 1.0},
    };
    Tri tris[] = {
        {{-2, -1.5, -2}, {2, -1.5, -2}, {2, -1.5, -6}},
    };
    Scene scene = {
        .vfov = 90.0,
        .aspect = (float) bmp->width / bmp->height,
        .spheres = spheres,
        .nspheres = sizeof(spheres) / sizeof(Sphere),
        .lights = lights,
        .nlights = sizeof(lights) / sizeof(Light),
        .background = {50, 100, 150},
        .ambient = 0.3,
        .tris = tris,
        .ntris = sizeof(tris) / sizeof(Tri),
    };
    float height = tan(torad(scene.vfov / 2)) * 2;
    float width = height * scene.aspect;
    for (int y = 0; y < bmp->height; y++) {
        for (int x = 0; x < bmp->width; x++) {
            int iy = bmp->height - y;
            Ray ray = {
                .orig = {0, 0, 0},
                .dir = {
                    (width * (x / (float)bmp->width)) - width / 2,
                    (height * (iy / (float)bmp->height)) - height / 2,
                    -1,
                },
            };
            ray.dir = vnorm(ray.dir);
            bmp->pixels[y * bmp->width + x] = cast(&ray, &scene, 0, 0);
        }
    }
}

static void rastercircle(Bitmap *bmp, Sphere *s) {
    float p = 2 * PI * s->radius;
    for (float i = 0; i < p; i++) {
        float rad = 2 * PI * i / p;
        float x = s->pos.x + s->radius * cos(rad);
        float y = s->pos.y + s->radius * sin(rad);
        y = bmp->height - y;
        if (x < 0 || x > bmp->width - 1) continue;
        if (y < 0 || y > bmp->height - 1) continue;
        bmp->pixels[(int)y * bmp->width + (int)x] = s->m.color;
    }
}

static void rasterline(Bitmap *bmp, Vec3f from, Vec3f to, Color c) {
    rastercircle(bmp, &(Sphere){from, 5, {c}});
    rastercircle(bmp, &(Sphere){to, 5, {c}});
    Vec3f v = vsub(to, from);
    float len = vmag(v);
    for (float i = 0; i < len; i++) {
        float p = i / len;
        float x = from.x + v.x * p;
        float y = from.y + v.y * p;
        y = bmp->height - y;
        if (x < 0 || x > bmp->width - 1) return;
        if (y < 0 || y > bmp->height - 1) return;
        bmp->pixels[(int)y * bmp->width + (int)x] = c;
    }
}

static void rasterray(Bitmap *bmp, Ray *r, Color c) {
    Vec3f to = vadd(r->orig, vmul(r->dir, 1e6));
    rasterline(bmp, r->orig, to, c);
}

static void raster(Bitmap *bmp) {
    clear(bmp, (Color){50, 100, 150});
    Sphere spheres[] = {
        {{100, 100, 0}, 50, {RED}},
        {{500, 200, 0}, 50, {RED}},
        {{300, 400, 0}, 50, {RED}},
    };
    Scene scene = {
        .spheres = spheres,
        .nspheres = sizeof(spheres)/sizeof(Sphere),
    };
    for (int i = 0; i < scene.nspheres; i++)
        rastercircle(bmp, &spheres[i]);
    Vec3f eye = {200, 100};
    Ray v = {eye, vnorm((Vec3f){100, 43.35})};
    Vec3f ipoint, in;
    float idist;
    Ray ray = v;
    int iter = 0;
    rasterray(bmp, &ray, YELLOW);
    while (intersect(&ray, &scene, &ipoint, &in, &idist)) {
        if (iter == 10) break;
        // rasterray(bmp, &ray, YELLOW);
        rasterline(bmp, ray.orig, ipoint, GREEN);
        rasterline(bmp, ipoint, vadd(ipoint, vmul(in, 100)), CYAN);
        Vec3f r = vrefl(vsub(ipoint, ray.orig), in);
        // rasterline(bmp, ipoint, vadd(ipoint, r), MAGENTA);
        ray.orig = vadd(ipoint, vmul(vnorm(r), 0.0001));
        ray.dir = vnorm(r);
        rasterray(bmp, &ray, MAGENTA);
        iter++;
    }
}

int main(int argc, char **argv) {
    printf("Hello, World!\n");

    for (int i = 1; i < argc; i++) {
        char *file = argv[i];
        printf("conf %s\n", file);
        Conf *conf = parseconf(file);
        dumpconf(conf);
        freeconf(conf);
    }
    return 0;

    suzanne = newobj("suzanne.obj");
    
    // prepare teapot
    TEAPOT.ntris = (TEAPOT_COUNT / 3) / 3;
    TEAPOT.tris = malloc(TEAPOT.ntris * sizeof(Tri));
    TEAPOT.pos = (Vec3f){2, 2, -4};
    TEAPOT.radius = 0.0;
    for (int i = 0; i < TEAPOT_COUNT; i += 9) {
        Tri *tri = &TEAPOT.tris[(i / 3) / 3];
        tri->a = (Vec3f){TEAPOT_VERTS[i + 0], TEAPOT_VERTS[i + 1], TEAPOT_VERTS[i + 2]};
        tri->b = (Vec3f){TEAPOT_VERTS[i + 3], TEAPOT_VERTS[i + 4], TEAPOT_VERTS[i + 5]};
        tri->c = (Vec3f){TEAPOT_VERTS[i + 6], TEAPOT_VERTS[i + 7], TEAPOT_VERTS[i + 8]};
        TEAPOT.radius = max(TEAPOT.radius, max(vmag(tri->a),
                max(vmag(tri->b), vmag(tri->c))));
        tri->a = vadd(tri->a, TEAPOT.pos);
        tri->b = vadd(tri->b, TEAPOT.pos);
        tri->c = vadd(tri->c, TEAPOT.pos);
    }

    Bitmap bmp;
    int width = 320;
    int height = 240;
    width *= 2;
    height *= 2;
    initbitmap(&bmp, width, height);
    clear(&bmp, (Color){0});
    render(&bmp);
    // raster(&bmp);
    output(&bmp, "output.ppm");
    freebitmap(&bmp);

    freeobj(suzanne);
    return 0;
}
