#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <png.h>
#include <setjmp.h>
#include <string.h>

typedef struct {
    uint8_t *data;
    size_t size;
    size_t off;
} InputState;

static int cleanup_and_return(int rc, InputState *in, FILE *fd, png_structp *png, png_infop *info) {
    if (png && *png) png_destroy_read_struct(png, info, NULL);
    if (fd) fclose(fd);
    if (in) {
        free(in->data);
        in->data = NULL;
        free(in);
    }
    return rc;
}

static void read_cb(png_structp png_ptr, png_bytep out, png_size_t n) {
    InputState *in = (InputState *)png_get_io_ptr(png_ptr);
    if (!in || n > in->size - in->off) {
        png_error(png_ptr, "read past end of input");
        return;
    }
    memcpy(out, in->data + in->off, (size_t)n);
    in->off += (size_t)n;
}

int main(int argc, char **argv){
    if (argc < 2) return 1;
    const char *path = argv[1];
    if (!path) return 1;
    png_structp png = NULL;
    png_infop info = NULL;

    InputState *in = malloc(sizeof(InputState));
    if (!in) return 1;
    in->data = NULL;
    FILE *fd = fopen(path, "rb");
    if (!fd) return cleanup_and_return(1, in, NULL, &png, &info);
    int err = fseek(fd, 0, SEEK_END);
    if (err) return cleanup_and_return(err, in, fd, &png, &info);
    long size = ftell(fd);
    if (size < 0) return cleanup_and_return(1, in, fd, &png, &info);
    err = fseek(fd, 0, SEEK_SET);
    if (err) return cleanup_and_return(err, in, fd, &png, &info);
    uint8_t *data = malloc(size);
    if (!data) return cleanup_and_return(1, in, fd, &png, &info);
    size_t read = fread(data, 1, size, fd);
    if (read != (size_t)size) return cleanup_and_return(1, in, fd, &png, &info);
    in->data = data;
    in->size = size;
    in->off = 0;

    png = png_create_read_struct(
        PNG_LIBPNG_VER_STRING,
        NULL,
        NULL,
        NULL
    );
    if(!png) return cleanup_and_return(1, in, fd, &png, &info);

    info = png_create_info_struct(png);
    if(!info) return cleanup_and_return(1, in, fd, &png, &info);

    if (setjmp(png_jmpbuf(png))) {
        return cleanup_and_return(0, in, fd, &png, &info);
    }
 
    png_set_read_fn(png, in, read_cb);
    png_read_info(png, info);

    return cleanup_and_return(0, in, fd, &png, &info);
}