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

static png_uint_32 u32_at(const FuzzInput *in, size_t off) {
    return ((png_uint_32)byte_at(in, off, 0) << 24) |
           ((png_uint_32)byte_at(in, off + 1, 0) << 16) |
           ((png_uint_32)byte_at(in, off + 2, 0) << 8) |
           byte_at(in, off + 3, 0);
}

static png_int_32 s32_at(const FuzzInput *in, size_t off) {
    return (png_int_32)u32_at(in, off);
}

static char *bounded_string(const FuzzInput *in, size_t off, size_t max_len, const char *fallback) {
    size_t fallback_len = strlen(fallback);
    if (off >= in->size) {
        char *out = malloc(fallback_len + 1);
        if (out) memcpy(out, fallback, fallback_len + 1);
        return out;
    }

    size_t n = in->size - off;
    if (n > max_len) n = max_len;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, in->data + off, n);
    out[n] = 0;
    if (out[0] == 0) {
        memcpy(out, fallback, fallback_len + 1);
    }
    return out;
}

static void set_base_ihdr(png_structp png, png_infop info, const FuzzInput *in) {
    static const int color_types[] = {
        PNG_COLOR_TYPE_GRAY,
        PNG_COLOR_TYPE_RGB,
        PNG_COLOR_TYPE_PALETTE,
        PNG_COLOR_TYPE_GRAY_ALPHA,
        PNG_COLOR_TYPE_RGB_ALPHA,
    };
    int color_type = color_types[byte_at(in, 0, 1) % 5];
    int bit_depth = 8;
    if (color_type == PNG_COLOR_TYPE_GRAY && (byte_at(in, 1, 0) & 1)) bit_depth = 16;
    if ((color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGB_ALPHA) && (byte_at(in, 1, 0) & 1)) bit_depth = 16;

    png_set_IHDR(
        png,
        info,
        1 + (u32_at(in, 2) % 256),
        1 + (u32_at(in, 6) % 256),
        bit_depth,
        color_type,
        (byte_at(in, 10, 0) & 1) ? PNG_INTERLACE_ADAM7 : PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE,
        PNG_FILTER_TYPE_BASE
    );
}

static void target_color_metadata(png_structp png, png_infop info, const FuzzInput *in) {
    png_color_16 background;
    png_color_8 sig_bit;
    png_uint_16 hist[256];
    png_color palette[256];
    memset(&background, 0, sizeof(background));
    memset(&sig_bit, 0, sizeof(sig_bit));

    background.index = byte_at(in, 11, 0);
    background.gray = (png_uint_16)u32_at(in, 12);
    background.red = (png_uint_16)u32_at(in, 16);
    background.green = (png_uint_16)u32_at(in, 20);
    background.blue = (png_uint_16)u32_at(in, 24);
    png_set_bKGD(png, info, &background);

    sig_bit.gray = 1 + (byte_at(in, 28, 7) & 15);
    sig_bit.red = 1 + (byte_at(in, 29, 7) & 15);
    sig_bit.green = 1 + (byte_at(in, 30, 7) & 15);
    sig_bit.blue = 1 + (byte_at(in, 31, 7) & 15);
    sig_bit.alpha = 1 + (byte_at(in, 32, 7) & 15);
    png_set_sBIT(png, info, &sig_bit);

    for (int i = 0; i < 256; ++i) {
        size_t off = 33 + (size_t)i * 5;
        palette[i].red = byte_at(in, off, (uint8_t)i);
        palette[i].green = byte_at(in, off + 1, (uint8_t)(255 - i));
        palette[i].blue = byte_at(in, off + 2, (uint8_t)(i ^ 0x33));
        hist[i] = ((png_uint_16)byte_at(in, off + 3, 0) << 8) | byte_at(in, off + 4, 1);
    }
    png_set_PLTE(png, info, palette, 1 + byte_at(in, 3, 15));
    png_set_hIST(png, info, hist);
}

static void target_physical_metadata(png_structp png, png_infop info, const FuzzInput *in) {
    png_set_gAMA_fixed(png, info, s32_at(in, 11));
    png_set_cHRM_fixed(
        png,
        info,
        s32_at(in, 15), s32_at(in, 19),
        s32_at(in, 23), s32_at(in, 27),
        s32_at(in, 31), s32_at(in, 35),
        s32_at(in, 39), s32_at(in, 43)
    );
    png_set_pHYs(png, info, u32_at(in, 47), u32_at(in, 51), byte_at(in, 55, 1) % 2);
    png_set_oFFs(png, info, s32_at(in, 56), s32_at(in, 60), byte_at(in, 64, 0) % 2);
    png_set_sRGB(png, info, byte_at(in, 65, 0) % 4);
}

static void target_profile_metadata(png_structp png, png_infop info, const FuzzInput *in) {
    char *name = bounded_string(in, 11, 31, "profile");
    char *profile = bounded_string(in, 43, 512, "icc payload");
    char *purpose = bounded_string(in, 556, 31, "calibration");
    char *units = bounded_string(in, 588, 31, "unit");
    char *p0 = bounded_string(in, 620, 31, "0");
    char *p1 = bounded_string(in, 652, 31, "1");
    if (!name || !profile || !purpose || !units || !p0 || !p1) goto out;

    png_charp params[2];
    params[0] = p0;
    params[1] = p1;

    png_set_iCCP(png, info, name, PNG_COMPRESSION_TYPE_BASE, profile, (png_uint_32)strlen(profile));
    png_set_pCAL(png, info, purpose, s32_at(in, 684), s32_at(in, 688), byte_at(in, 692, 0) % 4, 2, units, params);
#ifdef PNG_sCAL_SUPPORTED
    png_set_sCAL(
        png,
        info,
        byte_at(in, 693, 1) % 2,
        1.0 + (double)(u32_at(in, 694) % 100000) / 1000.0,
        1.0 + (double)(u32_at(in, 698) % 100000) / 1000.0
    );
#endif

out:
    free(name);
    free(profile);
    free(purpose);
    free(units);
    free(p0);
    free(p1);
}

static void target_time_and_unknowns(png_structp png, png_infop info, const FuzzInput *in) {
    png_time t;
    png_unknown_chunk chunks[4];
    memset(&t, 0, sizeof(t));
    memset(chunks, 0, sizeof(chunks));

    t.year = (png_uint_16)(1900 + (u32_at(in, 11) % 200));
    t.month = 1 + (byte_at(in, 15, 0) % 12);
    t.day = 1 + (byte_at(in, 16, 0) % 31);
    t.hour = byte_at(in, 17, 0) % 24;
    t.minute = byte_at(in, 18, 0) % 60;
    t.second = byte_at(in, 19, 0) % 61;
    png_set_tIME(png, info, &t);

    for (int i = 0; i < 4; ++i) {
        size_t off = 20 + (size_t)i * 40;
        for (int j = 0; j < 4; ++j) {
            uint8_t c = byte_at(in, off + (size_t)j, (uint8_t)("vpAg"[j]));
            chunks[i].name[j] = (png_byte)((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ? c : "vpAg"[j]);
        }
        chunks[i].name[4] = 0;
        chunks[i].data = in->size > off + 4 ? in->data + off + 4 : NULL;
        chunks[i].size = in->size > off + 4 ? (png_size_t)((in->size - off - 4) > 32 ? 32 : (in->size - off - 4)) : 0;
        chunks[i].location = byte_at(in, off + 36, 0);
    }

    png_set_keep_unknown_chunks(png, byte_at(in, 180, PNG_HANDLE_CHUNK_IF_SAFE) % 4, NULL, 0);
    png_set_unknown_chunks(png, info, chunks, 4);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;

    FuzzInput in;
    memset(&in, 0, sizeof(in));
    if (read_input(argv[1], &in) != 0) return 1;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        free_input(&in);
        return 1;
    }
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        free_input(&in);
        return 1;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        free_input(&in);
        return 0;
    }

    set_base_ihdr(png, info, &in);

    switch (byte_at(&in, 0, 0) & 3) {
        case 0: target_color_metadata(png, info, &in); break;
        case 1: target_physical_metadata(png, info, &in); break;
        case 2: target_profile_metadata(png, info, &in); break;
        default: target_time_and_unknowns(png, info, &in); break;
    }

    if (byte_at(&in, 181, 0) & 1) {
        png_free_data(png, info, PNG_FREE_ALL, -1);
    }

    png_destroy_read_struct(&png, &info, NULL);
    free_input(&in);
    return 0;
}
