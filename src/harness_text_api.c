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

static char *bounded_string(const FuzzInput *in, size_t off, size_t max_len, const char *fallback) {
    size_t fallback_len = strlen(fallback);
    if (off >= in->size) {
        char *out = malloc(fallback_len + 1);
        if (out) memcpy(out, fallback, fallback_len + 1);
        return out;
    }

    size_t n = in->size - off;
    if (n > max_len) n = max_len;
    size_t out_len = n > fallback_len ? n : fallback_len;
    char *out = malloc(out_len + 1);
    if (!out) return NULL;
    memcpy(out, in->data + off, n);
    out[n] = 0;
    if (out[0] == 0) {
        memcpy(out, fallback, fallback_len + 1);
    }
    return out;
}

static void target_text_api(png_structp png, png_infop info, const FuzzInput *in) {
    int compression;
    switch (byte_at(in, 1, 0) & 3) {
        case 0: compression = PNG_TEXT_COMPRESSION_NONE; break;
        case 1: compression = PNG_TEXT_COMPRESSION_zTXt; break;
        default: compression = PNG_TEXT_COMPRESSION_NONE; break;
    }

    char *key = bounded_string(in, 3, 31, "Comment");
    char *text = bounded_string(in, 35, 256, "first text");
    char *key2 = bounded_string(in, 292, 31, "Comment2");
    char *text2 = bounded_string(in, 324, 256, "second text");
    if (!key || !text || !key2 || !text2) goto out;

    png_text t;
    memset(&t, 0, sizeof(t));
    t.compression = compression;
    t.key = key;
    t.text = (byte_at(in, 2, 0) & 1) ? NULL : text;
    t.text_length = strlen(text);

    png_set_text(png, info, &t, 1);

    if (byte_at(in, 2, 0) & 8) {
        png_free_data(png, info, PNG_FREE_TEXT, -1);
    }

    memset(&t, 0, sizeof(t));
    t.compression = compression;
    t.key = key2;
    t.text = (byte_at(in, 2, 0) & 2) ? NULL : text2;
    t.text_length = strlen(text2);

    png_set_text(png, info, &t, 1);

out:
    free(key);
    free(text);
    free(key2);
    free(text2);
}

static void target_palette_api(png_structp png, png_infop info, const FuzzInput *in) {
    png_color palette[256];
    png_byte trans[256];
    png_color_16 trans_color;
    int num_palette = 1 + byte_at(in, 1, 0);
    int num_trans = byte_at(in, 2, 0);

    for (int i = 0; i < 256; ++i) {
        size_t off = 3 + (size_t)i * 4;
        palette[i].red = byte_at(in, off, (uint8_t)i);
        palette[i].green = byte_at(in, off + 1, (uint8_t)(255 - i));
        palette[i].blue = byte_at(in, off + 2, 0);
        trans[i] = byte_at(in, off + 3, 255);
    }

    memset(&trans_color, 0, sizeof(trans_color));
    trans_color.red = ((png_uint_16)byte_at(in, 1028, 0) << 8) | byte_at(in, 1029, 0);
    trans_color.green = ((png_uint_16)byte_at(in, 1030, 0) << 8) | byte_at(in, 1031, 0);
    trans_color.blue = ((png_uint_16)byte_at(in, 1032, 0) << 8) | byte_at(in, 1033, 0);
    trans_color.gray = ((png_uint_16)byte_at(in, 1034, 0) << 8) | byte_at(in, 1035, 0);

    png_set_PLTE(png, info, palette, num_palette);
    png_set_tRNS(png, info, trans, num_trans, &trans_color);
}

static void target_splt_api(png_structp png, png_infop info, const FuzzInput *in) {
    png_sPLT_entry entries[64];
    png_sPLT_t splt;
    char *name = bounded_string(in, 2, 31, "suggested");
    if (!name) return;

    for (int i = 0; i < 64; ++i) {
        size_t off = 34 + (size_t)i * 10;
        entries[i].red = ((png_uint_16)byte_at(in, off, 0) << 8) | byte_at(in, off + 1, 0);
        entries[i].green = ((png_uint_16)byte_at(in, off + 2, 0) << 8) | byte_at(in, off + 3, 0);
        entries[i].blue = ((png_uint_16)byte_at(in, off + 4, 0) << 8) | byte_at(in, off + 5, 0);
        entries[i].alpha = ((png_uint_16)byte_at(in, off + 6, 0) << 8) | byte_at(in, off + 7, 0);
        entries[i].frequency = ((png_uint_16)byte_at(in, off + 8, 0) << 8) | byte_at(in, off + 9, 0);
    }

    memset(&splt, 0, sizeof(splt));
    splt.name = name;
    splt.depth = (byte_at(in, 1, 8) & 1) ? 16 : 8;
    splt.entries = entries;
    splt.nentries = byte_at(in, 0, 1) & 63;
    png_set_sPLT(png, info, &splt, 1);
    free(name);
}

static void target_unknown_chunk_api(png_structp png, png_infop info, const FuzzInput *in) {
    png_unknown_chunk chunk;
    memset(&chunk, 0, sizeof(chunk));

    for (int i = 0; i < 4; ++i) {
        uint8_t c = byte_at(in, 1 + (size_t)i, (uint8_t)("vpAg"[i]));
        chunk.name[i] = (png_byte)((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ? c : "vpAg"[i]);
    }
    chunk.name[4] = 0;
    chunk.data = in->size > 5 ? in->data + 5 : NULL;
    chunk.size = in->size > 5 ? in->size - 5 : 0;
    chunk.location = byte_at(in, 0, 0);

    png_set_unknown_chunks(png, info, &chunk, 1);
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

    png_set_IHDR(
        png,
        info,
        16 + (byte_at(&in, 0, 0) & 31),
        16 + ((byte_at(&in, 1, 0) >> 3) & 31),
        8,
        PNG_COLOR_TYPE_RGB,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE,
        PNG_FILTER_TYPE_BASE
    );

    switch (byte_at(&in, 0, 0) & 3) {
        case 0: target_text_api(png, info, &in); break;
        case 1: target_palette_api(png, info, &in); break;
        case 2: target_splt_api(png, info, &in); break;
        default: target_unknown_chunk_api(png, info, &in); break;
    }

    png_destroy_read_struct(&png, &info, NULL);
    free_input(&in);
    return 0;
}
