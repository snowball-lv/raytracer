#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
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

typedef struct {
    Allocator *alloc;
    unsigned size;
    max_align_t _align[];
} MemHdr;

// #define DEBUG_ALLOC

#ifdef DEBUG_ALLOC
    #define printf(...) printf(__VA_ARGS__)
#else
    #define printf(...)
#endif

void *xmalloc(Allocator *a, unsigned size) {
    if (!size) return 0;
    MemHdr *hdr = malloc(sizeof(MemHdr) + size);
    printf("xmalloc %u [%s] %u -> %u\n", size, a->name, a->size, a->size + size);
    hdr->alloc = a;
    hdr->size = size;
    a->size += size;
    return hdr + 1;
}

void xfree(void *ptr) {
    if (!ptr) return;
    MemHdr *hdr = (MemHdr *)ptr - 1;
    printf("xfree %u [%s] %u -> %u\n", hdr->size, hdr->alloc->name,
            hdr->alloc->size, hdr->alloc->size - hdr->size);
    hdr->alloc->size -= hdr->size;
    free(hdr);
}

void *xrealloc(Allocator *a, void *ptr, unsigned size) {
    if (!ptr) return xmalloc(a, size);
    if (!size) { xfree(ptr); return 0; }
    MemHdr *hdr = (MemHdr *)ptr - 1;
    printf("xrealloc %u [%s]\n", size, a->name);
    hdr = realloc(hdr, sizeof(MemHdr) + size);
    // decrement size of old allocator by old size
    hdr->alloc->size -= hdr->size;
    // increment size of new allocator by new size
    hdr->alloc = a;
    hdr->size = size;
    a->size += size;
    return hdr + 1;
}
