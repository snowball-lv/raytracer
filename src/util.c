#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <raytracer/util.h>

void err(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("*** ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    exit(1);
}

char *readfile(const char *file) {
    FILE *f = fopen(file, "r");
    if (!f) err("no such file: %s", file);
    fseek(f, 0, SEEK_END);
    int size = ftell(f);
    char *buf = malloc(size + 1);
    rewind(f);
    int r = fread(buf, size, 1, f);
    fclose(f);
    buf[size] = 0;
    if (r == 1) return buf;
    free(buf);
    err("failed read: %s", file);
    return 0;
}
