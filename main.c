#include <raylib.h>
#include <raymath.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define COMPLEX_IMPL
#include "complex.h"
#include <math.h>

#define FPS 60
#define WIDHT 800
#define HEIGHT 600

#define streq(A, B) (strcmp(A, B) == 0)

typedef struct {
    uint32_t fsize;
    uint16_t formatty;
    uint16_t nchan;
    uint32_t samplert;
    uint32_t bytes_per_sec;
    uint16_t bytes_per_block;
    uint16_t bits_per_sample;
    uint32_t datasz;
    char* data;
} WaveFile;

static WaveFile AudioFile = { 0 };

void wavefile_free(WaveFile wav)
{
    if (wav.data) {
        free(wav.data);
    }
}

void pass_until(const char* pat, FILE* file)
{
    int c = 0;
    char* bufp = (char*)pat;
    while (1) {
        c = fgetc(file);
        if (c == EOF) {
            fprintf(stderr, "Failed reading header (premature end while looking for fmt data)\n");
            exit(-1);
        }
        if (c == *bufp) {
            bufp++;
        } else {
            bufp = (char*)pat;
        }
        if (*bufp == '\0') {
            break;
        }
    }
}

WaveFile load_wav_file(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Could not open file `%s` for reading\n", path);
        exit(-1);
    }
    WaveFile wav = { 0 };
    static char buf[64] = { 0 };
    if (fread(buf, 1, 4, f) != 4 || !streq(buf, "RIFF")) {
        fprintf(stderr, "Failed reading header (RIFF)\n");
        exit(-1);
    }

    if (fread(buf, 1, 4, f) != 4) {
        fprintf(stderr, "Failed reading header (file size)\n");
        exit(-1);
    }
    wav.fsize = *((uint32_t*)buf);

    if (fread(buf, 1, 4, f) != 4 || !streq(buf, "WAVE")) {
        fprintf(stderr, "Failed reading header (WAVE)\n");
        exit(-1);
    }
    memset(buf, 0, sizeof(buf));
    pass_until("fmt\x20", f);

    uint32_t fmtlen = 0;
    if (fread(buf, 1, 4, f) != 4) {
        fprintf(stderr, "Failed reading header (length of format data)\n");
        exit(-1);
    }
    fmtlen = *((uint32_t*)buf);

    if (fread(buf, 1, 2, f) != 2) {
        fprintf(stderr, "Failed reading header (type of format)\n");
        exit(-1);
    }
    wav.formatty = *((uint16_t*)buf);

    if (fread(buf, 1, 2, f) != 2) {
        fprintf(stderr, "Failed reading header (number of channels)\n");
        exit(-1);
    }
    wav.nchan = *((uint16_t*)buf);

    if (fread(buf, 1, 4, f) != 4) {
        fprintf(stderr, "Failed reading header (sample rate)\n");
        exit(-1);
    }
    wav.samplert = *((uint32_t*)buf);

    if (fread(buf, 1, 4, f) != 4) {
        fprintf(stderr, "Failed reading header (bytes per second)\n");
        exit(-1);
    }
    wav.bytes_per_sec = *((uint32_t*)buf);

    if (fread(buf, 1, 2, f) != 2) {
        fprintf(stderr, "Failed reading header (bytes per block)\n");
        exit(-1);
    }
    wav.bytes_per_block = *((uint16_t*)buf);

    if (fread(buf, 1, 2, f) != 2) {
        fprintf(stderr, "Failed reading header (bits per sample)\n");
        exit(-1);
    }
    wav.bits_per_sample = *((uint16_t*)buf);

    memset(buf, 0, sizeof(buf));
    pass_until("data", f);

    if (fread(buf, 1, 4, f) != 4) {
        fprintf(stderr, "Failed reading header (data size)\n");
        exit(-1);
    }
    wav.datasz = *((uint32_t*)buf);

    wav.data = calloc(wav.datasz, 1);

    int b = 0;
    for (size_t i = 1; i <= wav.datasz; i++) {
        b = fgetc(f);
        if (b == EOF) {
            fprintf(stdout, "WARN: Data segment size does not match declared size\n");
            break;
        }
        wav.data[i - 1] = (char)b;
    }

    fclose(f);
    return wav;
}

const char* get_filename(const char* path)
{
    size_t len = strlen(path);
    size_t i = len;
    for (; i > 0; i--) {
        if (path[i - 1] == '/' || path[i - 1] == '\\') {
            break;
        }
    }
    return path + i;
}

void audio_input_callback(void* buf, unsigned int frames)
{
    static size_t wav_data_ptr = 0;
    unsigned char* d = buf;
    for (size_t i = 0; i < frames; i++) {
        memcpy(d + (i * AudioFile.bytes_per_block), AudioFile.data + wav_data_ptr, AudioFile.bytes_per_block);
        wav_data_ptr += AudioFile.bytes_per_block;
        if (wav_data_ptr >= AudioFile.datasz)
            wav_data_ptr = 0;
    }
}

cmx* fft(cmx* x, size_t n)
{
    if (!n) {
        return NULL;
    }
    if (n == 1) {
        cmx* ret = calloc(n, sizeof(cmx));
        return ret;
    }
    cmx* x0 = calloc(n / 2, sizeof(cmx));
    cmx* x1 = calloc(n / 2, sizeof(cmx));

    for (size_t i = 0; 2 * i < n; i++) {
        x0[i] = x[2 * i];
        x0[i] = x[2 * i];
    }
    cmx* tmp = { 0 };
    tmp = fft(x0, n / 2);
    free(x0);
    x0 = tmp;
    tmp = fft(x1, n / 2);
    free(x1);
    x1 = tmp;

    cmx* ret = calloc(n, sizeof(cmx));
    for (size_t k = 0; k < (n / 2); k++) {
        cmx p = x0[k];
        cmx q_0 = cmx_mul(from_re(-2.0 * PI), from_im(1));
        cmx q_1 = cmx_div(q_0, from_re((double)n));
        cmx q_2 = cmx_mul(q_1, from_re((double)k));
        cmx q_3 = cmx_exp(q_2);
        cmx q = cmx_mul(q_3, x1[k + (n / 2)]);
        ret[k] = cmx_add(p, q);
        ret[k+(n/2)] = cmx_sub(p, q);
    }
    free(x0);
    free(x1);
    return ret;
}

int main(int argc, char** args)
{
    if (argc != 2) {
        fprintf(stderr, "Expected a .wav file path as an argument\n");
        exit(-1);
    }
    WaveFile wav = load_wav_file(args[1]);
    AudioFile = wav;
    InitWindow(WIDHT, HEIGHT, "FFT TEST");
    InitAudioDevice();
    SetTargetFPS(FPS);
    SetAudioStreamBufferSizeDefault(4096);

    AudioStream stream = LoadAudioStream(wav.samplert, wav.bits_per_sample, wav.nchan);

    SetAudioStreamCallback(stream, audio_input_callback);

    printf("WAV file\n"
           "fsize: %d\n"
           "formatty: %d\n"
           "nchan: %d\n"
           "samplert: %d\n"
           "bytes_per_sec: %d\n"
           "bytes_per_block: %d\n"
           "bits_per_sample: %d\n"
           "datasz: %d\n",
        wav.fsize,
        wav.formatty,
        wav.nchan,
        wav.samplert,
        wav.bytes_per_sec,
        wav.bytes_per_block,
        wav.bits_per_sample,
        wav.datasz);

    const char* filename = get_filename(args[1]);
    Vector2 fnsz = MeasureTextEx(GetFontDefault(), filename, 24, 10);
    Vector2 fnpos = {
        .x = WIDHT / 2.,
        .y = HEIGHT / 2.
    };
    fnpos = Vector2Subtract(fnpos, Vector2Scale(fnsz, .5));
    PlayAudioStream(stream);
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTextEx(GetFontDefault(), filename, fnpos, 24, 10, WHITE);
        EndDrawing();
    }
    UnloadAudioStream(stream);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
