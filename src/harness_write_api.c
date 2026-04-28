#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <png.h>
#include <setjmp.h>

typedef struct {
    uint8_t *data;
    size_t size;
} FuzzInput;

typedef struct {
    size_t bytes_written;
} WriteSink;

static void free_input(FuzzInput *in) {
    if (!in) return;
    free(in->data);
    in->data = NULL;
    in->size = 0;
}

static int read_input(const char *path, FuzzInput *in) {
    FILE *fd = fopen(path, "rb");
    if (!fd) return 1;
    if (fseek(fd, 0, SEEK_END) != 0) {
        fclose(fd);
        return 1;
    }
    long size = ftell(fd);
    if (size < 0) {
        fclose(fd);
        return 1;
    }
    if (fseek(fd, 0, SEEK_SET) != 0) {
        fclose(fd);
        return 1;
    }

    in->data = malloc((size_t)size + 1);
    if (!in->data) {
        fclose(fd);
        return 1;
    }
    if (fread(in->data, 1, (size_t)size, fd) != (size_t)size) {
        fclose(fd);
        free_input(in);
        return 1;
    }
    fclose(fd);
    in->data[size] = 0;
    in->size = (size_t)size;
    return 0;
}

static uint8_t byte_at(const FuzzInput *in, size_t off, uint8_t fallback) {
    return off < in->size ? in->data[off] : fallback;
}

static uint32_t u32_at(const FuzzInput *in, size_t off) {
    return ((uint32_t)byte_at(in, off, 0) << 24) |
           ((uint32_t)byte_at(in, off + 1, 0) << 16) |
           ((uint32_t)byte_at(in, off + 2, 0) << 8) |
           byte_at(in, off + 3, 0);
}

static void write_cb(png_structp png, png_bytep data, png_size_t len) {
    (void)data;
    WriteSink *sink = (WriteSink *)png_get_io_ptr(png);
    if (!sink || len > (png_size_t)(SIZE_MAX - sink->bytes_written)) {
        png_error(png, "write overflow");
        return;
    }
    sink->bytes_written += (size_t)len;
}

static void flush_cb(png_structp png) {
    (void)png;
}

static void fill_row(png_bytep row, png_size_t rowbytes, const FuzzInput *in, size_t seed_off) {
    for (png_size_t i = 0; i < rowbytes; ++i) {
        row[i] = byte_at(in, seed_off + (size_t)i, (uint8_t)(i * 33u + seed_off));
    }
}

static void set_palette(png_structp png, png_infop info, const FuzzInput *in) {
    png_color palette[256];
    for (int i = 0; i < 256; ++i) {
        size_t off = 64 + (size_t)i * 3;
        palette[i].red = byte_at(in, off, (uint8_t)i);
        palette[i].green = byte_at(in, off + 1, (uint8_t)(255 - i));
        palette[i].blue = byte_at(in, off + 2, (uint8_t)(i ^ 0x55));
    }
    png_set_PLTE(png, info, palette, 256);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;

    FuzzInput in;
    memset(&in, 0, sizeof(in));
    if (read_input(argv[1], &in) != 0) return 1;

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        free_input(&in);
        return 1;
    }
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        free_input(&in);
        return 1;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        free_input(&in);
        return 0;
    }

    WriteSink sink;
    memset(&sink, 0, sizeof(sink));
    png_set_write_fn(png, &sink, write_cb, flush_cb);

    static const struct {
        int color_type;
        int bit_depth;
    } modes[] = {
        { PNG_COLOR_TYPE_GRAY, 1 },
        { PNG_COLOR_TYPE_GRAY, 2 },
        { PNG_COLOR_TYPE_GRAY, 4 },
        { PNG_COLOR_TYPE_GRAY, 8 },
        { PNG_COLOR_TYPE_GRAY, 16 },
        { PNG_COLOR_TYPE_RGB, 8 },
        { PNG_COLOR_TYPE_RGB, 16 },
        { PNG_COLOR_TYPE_PALETTE, 8 },
        { PNG_COLOR_TYPE_GRAY_ALPHA, 8 },
        { PNG_COLOR_TYPE_RGB_ALPHA, 8 },
        { PNG_COLOR_TYPE_RGB_ALPHA, 16 },
    };

    size_t mode = byte_at(&in, 0, 0) % (sizeof(modes) / sizeof(modes[0]));
    png_uint_32 width = 1 + (u32_at(&in, 1) % 128);
    png_uint_32 height = 1 + (u32_at(&in, 5) % 16);
    int interlace = (byte_at(&in, 9, 0) & 1) ? PNG_INTERLACE_ADAM7 : PNG_INTERLACE_NONE;

    png_set_IHDR(
        png,
        info,
        width,
        height,
        modes[mode].bit_depth,
        modes[mode].color_type,
        interlace,
        PNG_COMPRESSION_TYPE_BASE,
        PNG_FILTER_TYPE_BASE
    );

    if (modes[mode].color_type == PNG_COLOR_TYPE_PALETTE) {
        set_palette(png, info, &in);
    }

    png_color_8 sig_bits;
    memset(&sig_bits, 0, sizeof(sig_bits));
    sig_bits.red = 1 + (byte_at(&in, 10, 7) & 15);
    sig_bits.green = 1 + (byte_at(&in, 11, 7) & 15);
    sig_bits.blue = 1 + (byte_at(&in, 12, 7) & 15);
    sig_bits.gray = 1 + (byte_at(&in, 13, 7) & 15);
    sig_bits.alpha = 1 + (byte_at(&in, 14, 7) & 15);

    uint8_t flags = byte_at(&in, 15, 0);
    if (flags & 0x01) png_set_bgr(png);
    if (flags & 0x02) png_set_swap(png);
    if (flags & 0x04) png_set_packing(png);
    if (flags & 0x08) png_set_packswap(png);
    if (flags & 0x10) png_set_invert_mono(png);
    if (flags & 0x20) png_set_shift(png, &sig_bits);
    if (flags & 0x40) png_set_filler(png, u32_at(&in, 16), (byte_at(&in, 20, 0) & 1) ? PNG_FILLER_AFTER : PNG_FILLER_BEFORE);

    png_set_filter(png, PNG_FILTER_TYPE_BASE, byte_at(&in, 21, PNG_ALL_FILTERS));
    png_set_compression_level(png, byte_at(&in, 22, 6) % 10);
    png_set_compression_mem_level(png, 1 + (byte_at(&in, 23, 8) % 9));
    png_set_compression_strategy(png, byte_at(&in, 24, 0) % 4);
    png_set_compression_window_bits(png, 8 + (byte_at(&in, 25, 7) % 8));

    png_write_info(png, info);

    png_size_t rowbytes = png_get_rowbytes(png, info);
    if (rowbytes == 0 || rowbytes > 8192) {
        png_destroy_write_struct(&png, &info);
        free_input(&in);
        return 0;
    }

    png_bytep row = malloc(rowbytes);
    if (!row) {
        png_destroy_write_struct(&png, &info);
        free_input(&in);
        return 1;
    }

    for (png_uint_32 y = 0; y < height; ++y) {
        fill_row(row, rowbytes, &in, 32 + (size_t)y * 17);
        png_write_row(png, row);
    }

    free(row);
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    free_input(&in);
    return 0;
}
