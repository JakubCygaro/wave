#include "wavfile.h"
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
static WaveFile audio_file = { 0 };
static AudioStream audio_stream = { 0 };
static int is_playing = 0;
static char* message = NULL;

void audio_input_callback(void* buf, unsigned int frames);

static int try_load_wav(const char* path)
{
    if(is_playing){
        UnloadAudioStream(audio_stream);
        wav_data_ptr = 0;
        wavefile_free(audio_file);
        audio_file = (WaveFile){ 0 };
    }
    is_playing = 0;
    if (IsPathFile(path)) {
        if (message) {
            free(message);
            message = NULL;
        }
        if (!load_wav_file(path, &audio_file)) {
            message = wav_get_error();
            return 0;
        } else {
            const char* name = GetFileName(path);
            message = calloc(strlen(name) + 1, sizeof(char));
            strcpy(message, name);
        }
    }
    audio_stream = LoadAudioStream(audio_file.samplert, audio_file.bits_per_sample, audio_file.nchan);
    SetAudioStreamCallback(audio_stream, audio_input_callback);
    PlayAudioStream(audio_stream);
    is_playing = 1;

    printf("WAV file\n"
           "fsize: %d\n"
           "formatty: %d\n"
           "nchan: %d\n"
           "samplert: %d\n"
           "bytes_per_sec: %d\n"
           "bytes_per_block: %d\n"
           "bits_per_sample: %d\n"
           "datasz: %d\n",
        audio_file.fsize,
        audio_file.formatty,
        audio_file.nchan,
        audio_file.samplert,
        audio_file.bytes_per_sec,
        audio_file.bytes_per_block,
        audio_file.bits_per_sample,
        audio_file.datasz);
    return 1;
}

void audio_input_callback(void* buf, unsigned int frames)
{
    unsigned char* d = buf;
    last_data_ptr = wav_data_ptr;
    for (size_t i = 0; i < frames; i++) {
        memcpy(d + (i * audio_file.bytes_per_block), audio_file.data + wav_data_ptr, audio_file.bytes_per_block);

        wav_data_ptr += audio_file.bytes_per_block;
        if (wav_data_ptr >= audio_file.datasz)
            wav_data_ptr = 0;
    }
    last_frames = frames;
}
void update(void)
{
    if (IsFileDropped()) {
        FilePathList pl = LoadDroppedFiles();
        try_load_wav(pl.paths[0]);
    }
    if (!last_frames || !cmx_pre || !is_playing)
        return;
    size_t sample_rate = (last_frames / FFTSIZE);
    for (size_t i = 0; i < FFTSIZE && i < last_frames; i++) {
        // unsigned short v = *(unsigned short*)(AudioFile.data + last_data_ptr + (i * AudioFile.bytes_per_block * 8));
        unsigned int v = *(unsigned int*)(audio_file.data + last_data_ptr + (i * audio_file.bytes_per_block * 32));
        fft_input[i] = cmx_re((double)v);
    }
    (void)cmx_fft2(fft_input, FFTSIZE, 0, cmx_pre, fft_output);
    for (size_t i = 0; i < FFTSIZE; i++) {
        cmx c = fft_output[i];
        double v = cmx_mod(cmx_mul(c, cmx_re((i + 1) * 0.5)));
        max = v > max ? v : max;
        fft_draw_data[i] = v;
    }
    max *= 1.05;
}
void draw(void)
{
    Vector2 fnsz = MeasureTextEx(GetFontDefault(), message, 24, 10);
    Vector2 fnpos = {
        .x = WIDHT / 2.,
        .y = HEIGHT / 2.
    };
    fnpos = Vector2Subtract(fnpos, Vector2Scale(fnsz, .5));
    DrawTextEx(GetFontDefault(), message, fnpos, 24, 10, WHITE);
    if(!is_playing) return;
    const float cw = (float)WIDHT / (float)((FFTSIZE)-1);
    for (size_t i = 0; i < (FFTSIZE)-1; i++) {
        double c = fft_draw_data[i + 1];
        double v = (c / max) * (double)HEIGHT;
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
    cmx_pre = cmx_precomp_reversed_bits(FFTSIZE);
    fft_input = calloc(FFTSIZE, sizeof(cmx));
    fft_output = calloc(FFTSIZE, sizeof(cmx));
    fft_draw_data = calloc(FFTSIZE, sizeof(double));

    InitWindow(WIDHT, HEIGHT, "FFT TEST");
    InitAudioDevice();
    SetTargetFPS(FPS);
    SetAudioStreamBufferSizeDefault(4096);
    if (argc == 2) {
        char* path = args[1];
        try_load_wav(path);
    }
    while (!WindowShouldClose()) {
        update();
        BeginDrawing();
        ClearBackground(BLACK);
        draw();
        EndDrawing();
    }
    free(fft_input);
    free(fft_output);
    free(fft_draw_data);
    UnloadAudioStream(audio_stream);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
