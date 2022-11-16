#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <raytracer/math.h>
#include <raytracer/obj.h>
#include <raytracer/raytracer.h>
#include <raytracer/conf.h>

#define PI 3.14159265358979323846
#define MAX_RECUR 1

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

static void output(Bitmap *bmp, const char *file) {
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

static Vec3 vclamp(Vec3 v) {
    return vec3(clamp(v.x, 0, 1), clamp(v.y, 0, 1), clamp(v.z, 0, 1));
}

static Vec3 xcast(Scene *s, Ray *r, int recur, Hit *xhit) {
    xhit->dist = FLT_MAX;
    Hit hit;
    if (!testscene(s, r, &hit)) return s->background;
    *xhit = hit;
    Vec3 diffuse = hit.shape->mat.diffuse;
    Vec3 specular = vec3(1.0, 1.0, 1.0);
    Vec3 ambient = s->background;
    Vec3 color = vec3(0.0, 0.0, 0.0);
    Vec3 v = vsub(r->orig, hit.point);
    Vec3 vr = vrefl(vsub(hit.point, r->orig), hit.norm);

    // lights
    for (int k = 0; k < s->nlights; k++) {
        Light *light = s->lights[k];
        Vec3 l = vsub(light->pos, hit.point);
        // occlusion
        {
            Ray ray = {hit.point, vnorm(l)};
            ray.orig = vadd(ray.orig, vmul(ray.dir, 0.0001));
            Hit lh;
            if (testscene(s, &ray, &lh))
                if (lh.dist < vmag(l))
                    continue;
        }
        float attenuation = 1.0 / vmag(l);
        float ilight = light->intensity * attenuation;
        // diffuse
        {
            float idiffuse = vdot(vnorm(l), hit.norm);
            idiffuse = clamp(idiffuse, 0.0, 1.0);
            idiffuse *= ilight;
            color = vadd(color, vmul(diffuse, idiffuse));
        }
        // specular
        {
            Vec3 lr = vrefl(vsub(hit.point, light->pos), hit.norm);
            float ispecular = vdot(vnorm(v), vnorm(lr));
            ispecular = clamp(ispecular, 0.0, 1.0);
            ispecular = pow(ispecular, 16);
            ispecular *= ilight;
            color = vadd(color, vmul(specular, ispecular));
        }
    }

    // reflections
    if (recur < MAX_RECUR) {
        Ray ray = {hit.point, vnorm(vr)};
        ray.orig = vadd(ray.orig, vmul(hit.norm, 0.0001));
        Hit rhit;
        Vec3 rc = xcast(s, &ray, recur + 1, &rhit);
        float reflectiveness = hit.shape->mat.reflectiveness;
        // float attenuation = 1.0 / rhit.dist;
        // attenuation = clamp(attenuation, 0.0, 1.0);
        // float irefl = attenuation * reflectiveness;
        float irefl = reflectiveness;
        color = vadd(color, vmul(rc, irefl));
    }

    return color;
}

static Color cast(Ray *r, Scene *s) {
    Hit hit;
    Vec3 fc = xcast(s, r, 0, &hit);
    fc = vclamp(fc);
    return (Color){255 * fc.x, 255 * fc.y, 255 * fc.z};
}

static void renderscene(Bitmap *bmp, Scene *scene) {
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
            bmp->pixels[y * bmp->width + x] = cast(&ray, scene);
        }
    }
}

int main(int argc, char **argv) {
    printf("Hello, World!\n");

    for (int i = 1; i < argc; i++) {
        Scene *s = newscene(argv[i]);
        Bitmap bmp;
        initbitmap(&bmp, s->width, s->height);
        clear(&bmp, (Color){0});
        renderscene(&bmp, s);
        output(&bmp, s->output);
        freebitmap(&bmp);
        freescene(s);
    }

    return 0;
}
