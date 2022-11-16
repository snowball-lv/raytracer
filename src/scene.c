#include <stdlib.h>
#include <string.h>
#include <raytracer/math.h>
#include <raytracer/obj.h>
#include <raytracer/raytracer.h>
#include <raytracer/conf.h>

void addshape(Scene *s, Shape *shape) {
    s->nshapes++;
    s->shapes = realloc(s->shapes, s->nshapes * sizeof(Shape *));
    s->shapes[s->nshapes - 1] = shape;
}

static void addlight(Scene *s, Light *light) {
    s->nlights++;
    s->lights = realloc(s->lights, s->nlights * sizeof(Light *));
    s->lights[s->nlights - 1] = light;
}

static Vec3 getvec(ConfVal *obj, const char *name) {
    ConfVal *val = confobjget(obj, name);
    if (!val || val->type != CONF_ARR || confarrsize(val) != 3)
        return vec3(0, 0, 0);
    return vec3(
        confarrgetnum(val, 0, 0),
        confarrgetnum(val, 1, 0),
        confarrgetnum(val, 2, 0));
}

static void loadmaterial(Material *m, ConfVal *obj) {
    m->diffuse = getvec(obj, "diffuse");
    m->reflectiveness = confobjgetnum(obj, "reflectiveness", 0);
}

static void loadshape(Scene *s, ConfVal *shape) {
    if (shape->type != CONF_OBJ) return;
    const char *type = confobjgetstr(shape, "type", "");
    // const char *objfile = confobjgetstr(shape, "objfile", "");
    if (strcmp(type, "sphere") == 0) {
        Vec3 position = getvec(shape, "position");
        float radius = confobjgetnum(shape, "radius", 1);
        ShapeSphere *sphere = newsphere(position, radius);
        addshape(s, AS_SHAPE(sphere));
        ConfVal *material = confobjget(shape, "material");
        if (material)
            loadmaterial(&sphere->shape.mat, material);
    }
    else if (strcmp(type, "plane") == 0) {
        Vec3 point = getvec(shape, "point");
        Vec3 normal = getvec(shape, "normal");
        ShapePlane *plane = newplane(point, normal);
        addshape(s, AS_SHAPE(plane));
        ConfVal *material = confobjget(shape, "material");
        if (material)
            loadmaterial(&plane->shape.mat, material);
    }
    else if (strcmp(type, "mesh") == 0) {
        const char *objfile = confobjgetstr(shape, "objfile", 0);
        if (!objfile) return;
        Obj *obj = newobj(objfile);
        if (!obj) return;
        ShapeMesh *mesh = newmesh(obj);
        Vec3 position = getvec(shape, "position");
        shapetranslate(AS_SHAPE(mesh), position);
        addshape(s, AS_SHAPE(mesh));
        ConfVal *material = confobjget(shape, "material");
        if (material)
            loadmaterial(&mesh->shape.mat, material);
    }
}

static void loadlight(Scene *s, ConfVal *light) {
    Vec3 position = getvec(light, "position");
    float intensity = confobjgetnum(light, "intensity", 1);
    Light *l = malloc(sizeof(Light));
    memset(l, 0, sizeof(Light));
    l->pos = position;
    l->intensity = intensity;
    addlight(s, l);
}

static const char *mkstrcpy(const char *str) {
    char *cpy = malloc(strlen(str) + 1);
    strcpy(cpy, str);
    return cpy;
}

static void load(Scene *s, Conf *conf) {
    s->width = confobjgetnum(conf->root, "width", 640);
    s->height = confobjgetnum(conf->root, "height", 480);
    s->output = mkstrcpy(confobjgetstr(conf->root, "output", "out.ppm"));
    s->vfov = confobjgetnum(conf->root, "vfov", 90);
    s->aspect = (float)s->width / s->height;
    s->background = getvec(conf->root, "background");
    ConfVal *shapes = confobjget(conf->root, "shapes");
    int nshapes = confarrsize(shapes);
    for (int i = 0; i < nshapes; i++)
        loadshape(s, confarrget(shapes, i));
    ConfVal *lights = confobjget(conf->root, "lights");
    int nlights = confarrsize(lights);
    for (int i = 0; i < nlights; i++)
        loadlight(s, confarrget(lights, i));
}

Scene *newscene(const char *file) {
    Scene *s = malloc(sizeof(Scene));
    memset(s, 0, sizeof(Scene));
    if (!file) return s;
    Conf *conf = parseconf(file);
    if (!conf) return s;
    dumpconf(conf);
    load(s, conf);
    freeconf(conf);
    return s;
}

void freescene(Scene *s) {
    free((void *)s->output);
    for (int i = 0; i < s->nshapes; i++)
        freeshape(s->shapes[i]);
    if (s->shapes) free(s->shapes);
    for (int i = 0; i < s->nlights; i++)
        free(s->lights[i]);
    if (s->lights) free(s->lights);
    free(s);
}
