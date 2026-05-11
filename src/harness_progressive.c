#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <png.h>
#include <setjmp.h>
#include <string.h>

#define MAX_IMAGE_DIMENSION 4096U
#define MAX_DECODED_BYTES (16U * 1024U * 1024U)

typedef struct {
    uint8_t *data;
    size_t size;
    png_bytep row;
    png_uint_32 height;
    png_size_t rowbytes;
} ProgressiveState;

static int cleanup_and_return(
    int rc,
    ProgressiveState *state,
    FILE *fd,
    png_structp *png,
    png_infop *info
) {
    if (png && *png) png_destroy_read_struct(png, info, NULL);
    if (fd) fclose(fd);
    if (state) {
        free(state->row);
        free(state->data);
        free(state);
    }
    return rc;
}

static void progressive_info_cb(png_structp png, png_infop info) {
    ProgressiveState *state = (ProgressiveState *)png_get_progressive_ptr(png);
    png_uint_32 width = png_get_image_width(png, info);
    png_uint_32 height = png_get_image_height(png, info);

    if (!state || width > MAX_IMAGE_DIMENSION || height > MAX_IMAGE_DIMENSION) {
        png_error(png, "image dimensions exceed harness limit");
        return;
    }

    png_set_expand(png);
    png_set_strip_16(png);
    png_set_gray_to_rgb(png);

    png_color_16 bg = {0, 0, 0, 0, 0};
    png_set_background(png, &bg, PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
    png_set_gamma(png, 2.2, 0.45455);

    png_set_interlace_handling(png);
    png_read_update_info(png, info);
    png_start_read_image(png);

    state->height = height;
    state->rowbytes = png_get_rowbytes(png, info);
    if (state->height == 0 || state->rowbytes == 0 ||
        state->rowbytes > MAX_DECODED_BYTES / (size_t)state->height) {
        png_error(png, "decoded image exceeds harness limit");
        return;
    }

    state->row = malloc(state->rowbytes);
    if (!state->row) {
        png_error(png, "row allocation failed");
        return;
    }
    memset(state->row, 0, state->rowbytes);
}

static void progressive_row_cb(
    png_structp png,
    png_bytep new_row,
    png_uint_32 row_num,
    int pass
) {
    (void)row_num;
    (void)pass;
    ProgressiveState *state = (ProgressiveState *)png_get_progressive_ptr(png);
    if (!state || !state->row) {
        png_error(png, "row before info callback");
        return;
    }
    png_progressive_combine_row(png, state->row, new_row);
}

static void progressive_end_cb(png_structp png, png_infop info) {
    (void)png;
    (void)info;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    const char *path = argv[1];
    if (!path) return 1;

    png_structp png = NULL;
    png_infop info = NULL;
    ProgressiveState *state = malloc(sizeof(*state));
    if (!state) return 1;
    memset(state, 0, sizeof(*state));

    FILE *fd = fopen(path, "rb");
    if (!fd) return cleanup_and_return(1, state, NULL, &png, &info);
    if (fseek(fd, 0, SEEK_END)) return cleanup_and_return(1, state, fd, &png, &info);
    long size = ftell(fd);
    if (size < 0) return cleanup_and_return(1, state, fd, &png, &info);
    if (fseek(fd, 0, SEEK_SET)) return cleanup_and_return(1, state, fd, &png, &info);

    state->data = malloc((size_t)size);
    if (!state->data) return cleanup_and_return(1, state, fd, &png, &info);
    if (fread(state->data, 1, (size_t)size, fd) != (size_t)size)
        return cleanup_and_return(1, state, fd, &png, &info);
    state->size = (size_t)size;

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) return cleanup_and_return(1, state, fd, &png, &info);

    info = png_create_info_struct(png);
    if (!info) return cleanup_and_return(1, state, fd, &png, &info);

    if (setjmp(png_jmpbuf(png))) {
        return cleanup_and_return(0, state, fd, &png, &info);
    }

    png_set_progressive_read_fn(
        png,
        state,
        progressive_info_cb,
        progressive_row_cb,
        progressive_end_cb
    );

    for (size_t off = 0; off < state->size;) {
        size_t chunk = 1 + ((off * 13U) & 31U);
        if (chunk > state->size - off) chunk = state->size - off;
        png_process_data(png, info, state->data + off, chunk);
        off += chunk;
    }

    return cleanup_and_return(0, state, fd, &png, &info);
}
