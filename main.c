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
#define FFTSIZE (1 << 10)
#define PROGRESS_BAR_H (1. / 18.)

static int w_width = WIDHT;
static int w_height = HEIGHT;
static float pb_height = PROGRESS_BAR_H * HEIGHT;
static int* cmx_pre = NULL;
static cmx* fft_input = NULL;
static cmx* fft_output = NULL;
static double* fft_draw_data = NULL;
static unsigned int last_frames = 0;
static size_t wav_data_ptr = 0;
static size_t last_data_ptr = 0;
static double max = 0;
static WaveFile audio_file = { 0 };
static AudioStream audio_stream = { 0 };
static int is_playing = 0;
static char* message = NULL;

void audio_input_callback(void* buf, unsigned int frames);

static int try_load_wav(const char* path)
{
    if (is_playing) {
        UnloadAudioStream(audio_stream);
        wav_data_ptr = 0;
        wavefile_free(audio_file);
        audio_file = (WaveFile) { 0 };
        memset(fft_draw_data, 0, FFTSIZE * sizeof(double));
        memset(fft_input, 0, FFTSIZE * sizeof(cmx));
        memset(fft_draw_data, 0, FFTSIZE * sizeof(cmx));
        max = 0.0;
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
        if (audio_file.formatty != 1) {
            const char* msg = "unsupported format (compression)";
            message = calloc(strlen(msg) + 1, sizeof(char));
            strcpy(message, msg);
        }
    }
    audio_stream = LoadAudioStream(audio_file.samplert, audio_file.bits_per_sample, audio_file.nchan);
    SetAudioStreamCallback(audio_stream, audio_input_callback);
    PlayAudioStream(audio_stream);
    is_playing = 1;

    TraceLog(LOG_INFO, "[WAV FILE]\n"
                       "- File size: %d\n"
                       "- Format type: %d\n"
                       "- Number of channels: %d\n"
                       "- Sample rate: %d\n"
                       "- Bytes per second: %d\n"
                       "- Bytes per block: %d\n"
                       "- Bits per sample: %d\n"
                       "- WAV data size: %d",
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

void update_progress_bar(void)
{
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        return;
    const size_t whole = audio_file.datasz;
    const float h = PROGRESS_BAR_H * w_height;
    Rectangle r = { .x = 0.0, .y = w_height - h, .width = w_width, .height = h };
    Vector2 mouse = GetMousePosition();
    if(!CheckCollisionPointRec(mouse, r)) return;
    const float ratio = (float)mouse.x / (float)w_width;
    size_t pos = whole * ratio;
    pos -= (pos % audio_file.bytes_per_sec);
    wav_data_ptr = pos;
}
void update(void)
{
    if (IsWindowResized()) {
        w_height = GetScreenHeight(), w_width = GetScreenWidth();
        pb_height = PROGRESS_BAR_H * w_height;
    }
    if (IsFileDropped()) {
        FilePathList pl = LoadDroppedFiles();
        try_load_wav(pl.paths[0]);
        UnloadDroppedFiles(pl);
    }
    if (!last_frames || !cmx_pre || !is_playing)
        return;
    update_progress_bar();

    size_t sample_rate = audio_file.bytes_per_block * 2;
    for (size_t i = 0; i < FFTSIZE; i++) {
        double v = 0;
        long int left = 0;
        long int right = 0;
        uint16_t nchan = audio_file.nchan;
        switch (audio_file.bytes_per_block) {
        case 2: {
            unsigned char* left_p = (unsigned char*)(audio_file.data + last_data_ptr + (i * audio_file.bytes_per_block * sample_rate));
            left = *left_p;
            right = (nchan == 2) ? *(left_p + 1) : left;
        } break;
        case 4: {
            short* left_p = (short*)(audio_file.data + last_data_ptr + (i * audio_file.bytes_per_block * sample_rate));
            left = *left_p;
            right = (nchan == 2) ? *(left_p + 1) : left;
        } break;
        case 8: {
            int* left_p = (int*)(audio_file.data + last_data_ptr + (i * audio_file.bytes_per_block * sample_rate));
            left = *left_p;
            right = (nchan == 2) ? *(left_p + 1) : left;
        } break;
        }
        v = ((double)left + (double)right) * 0.5f / (500.f);
        fft_input[i] = cmx_re(v);
    }
    (void)cmx_fft2(fft_input, FFTSIZE, 0, cmx_pre, fft_output);
    max = 0.0;
    for (size_t i = 0; i < FFTSIZE; i++) {
        cmx c = fft_output[i];
        // double v = cmx_mod(cmx_mul(c, cmx_re((i + 1) * 0.5)));
        double v = cmx_mod(c);
        max = v > max ? v : max;
        fft_draw_data[i] = v;
    }
}
void draw_progress_bar(void)
{
    const size_t whole = audio_file.datasz;
    const float ratio = (float)wav_data_ptr / (float)whole;
    const float pos = w_width * ratio;
    const float h = PROGRESS_BAR_H * w_height;
    DrawRectangleV(
        (Vector2) { .x = 0.0, .y = w_height - h },
        (Vector2) { .x = pos, .y = h },
        WHITE);
}
void draw(void)
{
    Vector2 fnsz = MeasureTextEx(GetFontDefault(), message, 24, 10);
    Vector2 fnpos = {
        .x = w_width / 2.,
        .y = w_height / 2.
    };
    fnpos = Vector2Subtract(fnpos, Vector2Scale(fnsz, .5));
    DrawTextEx(GetFontDefault(), message, fnpos, 24, 10, WHITE);
    if (!is_playing)
        return;
    draw_progress_bar();
    const float cw = (float)w_width / (float)(FFTSIZE / 2.);
    const size_t whole = FFTSIZE / 2;
    const double total_height = (double)(w_height - pb_height);
    for (size_t i = 0; i < whole; i++) {
        double c = fft_draw_data[i];
        double v = (c / max) * total_height;
        Rectangle r = {
            .x = cw * i,
            .y = total_height - v,
            .width = cw,
            .height = v,
        };
        DrawRectangleRec(r, ColorFromHSV(((float)i / (float)whole) * 360., 1.0, 1.0));
    }
}

int main(int argc, char** args)
{
    cmx_pre = cmx_precomp_reversed_bits(FFTSIZE);
    fft_input = calloc(FFTSIZE, sizeof(cmx));
    fft_output = calloc(FFTSIZE, sizeof(cmx));
    fft_draw_data = calloc(FFTSIZE, sizeof(double));

    InitWindow(w_width, w_height, "FFT TEST");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
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
    free(cmx_pre);
    if (message)
        free(message);
    UnloadAudioStream(audio_stream);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
