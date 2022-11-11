#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <raytracer/obj.h>
#include <raytracer/raytracer.h>
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

typedef struct {
    Color color;
} Material;

typedef struct {
    Vec3f pos;
    float radius;
    Material m;
} Sphere;

typedef struct {
    Vec3f pos;
    float intensity;
} Light;

typedef struct {
    Tri *tris;
    int ntris;
    Vec3f pos;
    float radius;
} Mesh;

struct Scene {
    float vfov;
    float aspect;
    Light *lights;
    int nlights;
    Color background;
    float ambient;
    Shape **shapes;
    int nshapes;
};

static void addshape(Scene *s, Shape *shape) {
    s->nshapes++;
    s->shapes = realloc(s->shapes, s->nshapes * sizeof(Shape *));
    s->shapes[s->nshapes - 1] = shape;
}

static float torad(float deg) {
    return deg * PI / 180.0;
}

static int testscene(Scene *s, Ray *r, Hit *h) {
    Hit tmp;
    float last_dist = FLT_MAX;
    int success = 0;
    for (int i = 0; i < s->nshapes; i++) {
        Shape *shape = s->shapes[i];
        if (!testshape(shape, r, &tmp)) continue;
        if (tmp.dist > last_dist) continue;
        last_dist = tmp.dist;
        *h = tmp;
        success = 1;
    }
    return success;
}

static Vec3 xcast(Scene *s, Ray *r, int recur) {
    Hit hit;
    if (!testscene(s, r, &hit)) return vec3(0.2, 0.3, 0.4);
    Vec3 color = vec3(1.0, 1.0, 0.0);
    Vec3f v = vsub(r->orig, hit.point);
    Vec3f vr = vrefl(vsub(hit.point, r->orig), hit.norm);
    
    // lights
    float intensity = s->ambient;
    for (int k = 0; k < s->nlights; k++) {
        Light *light = &s->lights[k];
        Vec3f l = vsub(light->pos, hit.point);
        // occlusion
        {
            Ray ray = {hit.point, vnorm(l)};
            ray.orig = vadd(ray.orig, vmul(ray.dir, 0.0001));
            Hit lh;
            if (testscene(s, &ray, &lh))
                if (lh.dist < vmag(l))
                    continue;
        }
        Vec3f r = vrefl(vsub(hit.point, light->pos), hit.norm);
        float light_i = vdot(vnorm(l), hit.norm) * light->intensity;
        float refl = vdot(vnorm(v), vnorm(r));
        refl = clamp(refl, 0.0, 1.0);
        light_i += pow(refl, 16);
        intensity += clamp(light_i, 0.0, FLT_MAX);
    }

    // reflections
    if (recur < MAX_RECUR) {
        Ray ray = {hit.point, vnorm(vr)};
        ray.orig = vadd(ray.orig, vmul(hit.norm, 0.0001));
        Vec3 rc = xcast(s, &ray, recur + 1);
        color = vadd(color, vmul(rc, 0.5));
    }

    return vmul(color, intensity);
}

static Vec3 vclamp(Vec3 v) {
    return vec3(clamp(v.x, 0, 1), clamp(v.y, 0, 1), clamp(v.z, 0, 1));
}

static Color cast(Ray *r, Scene *s, int recur, int *hit) {
    Vec3 fc = xcast(s, r, recur);
    fc = vclamp(fc);
    return (Color){255 * fc.x, 255 * fc.y, 255 * fc.z};
}

static Scene *newscene() {
    Scene *s = malloc(sizeof(Scene));
    memset(s, 0, sizeof(Scene));
    return s;
}

static void freescene(Scene *s) {
    for (int i = 0; i < s->nshapes; i++)
        freeshape(s->shapes[i]);
    if (s->nshapes) free(s->shapes);
    free(s);
}

static void render(Bitmap *bmp) {
    Light lights[] = {
        {{-2, 2, 0}, 1.0},
        // {{0, 0, 0}, 1.0},
    };
    Scene *scene = newscene();
    scene->vfov = 90.0;
    scene->aspect = (float) bmp->width / bmp->height;
    scene->lights = lights;
    scene->nlights = sizeof(lights) / sizeof(Light);
    scene->background = (Color){50, 100, 150};
    scene->ambient = 0.3;

    addshape(scene, AS_SHAPE(newsphere(vec3(1, 0, -5), 1)));
    addshape(scene, AS_SHAPE(newsphere(vec3(-1, 0, -3), 1)));
    addshape(scene, AS_SHAPE(newsphere(vec3(1.5, -1, -6), 1)));
    addshape(scene, AS_SHAPE(newplane(vec3(-1, -1.5, 0), vec3(0.5, 1, 0))));

    ShapeMesh *suzanne = newmesh(newobj("suzanne.obj"));
    addshape(scene, AS_SHAPE(suzanne));

    float height = tan(torad(scene->vfov / 2)) * 2;
    float width = height * scene->aspect;
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
            bmp->pixels[y * bmp->width + x] = cast(&ray, scene, 0, 0);
        }
    }

    freescene(scene);
}

int main(int argc, char **argv) {
    printf("Hello, World!\n");

    Bitmap bmp;
    int width = 320;
    int height = 240;
    // width *= 2;
    // height *= 2;
    initbitmap(&bmp, width, height);
    clear(&bmp, (Color){0});
    render(&bmp);
    output(&bmp, "output.ppm");
    freebitmap(&bmp);

    return 0;
}
