#pragma once

void err(const char *fmt, ...);
char *readfile(const char *file);

typedef struct {
    const char *name;
    unsigned size;
} Allocator;

void *xmalloc(Allocator *a, unsigned size);
void xfree(void *ptr);
void *xrealloc(Allocator *a, void *ptr, unsigned size);
