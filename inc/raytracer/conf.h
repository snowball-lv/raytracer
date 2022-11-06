#pragma once

enum {
    CONF_NONE,
    CONF_NUM,
    CONF_STR,
    CONF_OBJ,
    CONF_ARR,
};

typedef struct ConfVal ConfVal;

typedef struct {
    ConfVal **keys;
    ConfVal **vals;
    int nkvs;
} ConfObj;

typedef struct {
    ConfVal **vals;
    int nvals;
} ConfArr;

struct ConfVal {
    int type;
    union {
        char *str;
        float num;
        ConfObj obj;
        ConfArr arr;
    } as;
};

typedef struct {
    ConfVal *root;
} Conf;

Conf *parseconf(const char *file);
void freeconf(Conf *conf);
void dumpconf(Conf *conf);
