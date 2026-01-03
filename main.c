#include <raylib.h>
#include <raymath.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define COMPLEX_IMPL
#include "complex.h"

#define FPS 60
#define WIDHT 800
#define HEIGHT 600
#define FFTSIZE 1 << 9

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
static int* cmx_pre = NULL;
static cmx* fft_input = NULL;
static cmx* fft_output = NULL;
static double* fft_draw_data = NULL;
static unsigned int last_frames = 0;
static size_t wav_data_ptr = 0;
static size_t last_data_ptr = 0;
static double max = WIDHT;
void audio_input_callback(void* buf, unsigned int frames)
{
    unsigned char* d = buf;
    last_data_ptr = wav_data_ptr;
    for (size_t i = 0; i < frames; i++) {
        memcpy(d + (i * AudioFile.bytes_per_block), AudioFile.data + wav_data_ptr, AudioFile.bytes_per_block);

        wav_data_ptr += AudioFile.bytes_per_block;
        if (wav_data_ptr >= AudioFile.datasz)
            wav_data_ptr = 0;
    }
    last_frames = frames;
}
void update(void)
{
    if (!last_frames || !cmx_pre)
        return;
    size_t sample_rate = (last_frames / FFTSIZE);
    for (size_t i = 0; i < FFTSIZE && i < last_frames; i++) {
        unsigned int v = *(unsigned int*)(AudioFile.data + last_data_ptr + (i * AudioFile.bytes_per_block * 10));
        // printf("%d\n", v);
        fft_input[i] = cmx_re((double)v);
    }
    (void)cmx_fft2(fft_input, FFTSIZE, cmx_pre, fft_output);
    for (size_t i = 0; i < FFTSIZE; i++) {
        cmx c = fft_output[i];
        // double v = cmx_mod(c);
        double v = cmx_mod(cmx_mul(c, cmx_re(i * 0.5)));
        max = v > max ? v : max;
        fft_draw_data[i] = v;
    }
    max *= 1.05;
}
void draw(void)
{
    const float cw = (float)WIDHT / (float)((FFTSIZE) - 1);
    for (size_t i = 0; i < (FFTSIZE) - 1; i++) {
        double c = fft_draw_data[i + 1];
        double v = (c / max) * (double)HEIGHT;
        // printf("%lf\n", v);
        Rectangle r = {
            .x = cw * i,
            .y = HEIGHT - v,
            .width = cw,
            .height = v,
        };
        DrawRectangleRec(r, RED);
    }
    max = 0.0;
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

    cmx_pre = cmx_precomp_reversed_bits(FFTSIZE);
    fft_input = calloc(FFTSIZE, sizeof(cmx));
    fft_output = calloc(FFTSIZE, sizeof(cmx));
    fft_draw_data = calloc(FFTSIZE, sizeof(double));

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
        update();
        BeginDrawing();
        ClearBackground(BLACK);
        draw();
        DrawTextEx(GetFontDefault(), filename, fnpos, 24, 10, WHITE);
        EndDrawing();
    }
    free(fft_input);
    free(fft_output);
    free(fft_draw_data);
    UnloadAudioStream(stream);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
