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
#define GROUPING_FACTOR (1 << 2)
#define COLUMN_COUNT (FFTSIZE / 2 / GROUPING_FACTOR)
#define SMOOTHING .5f

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
static double min = 0;
static WaveFile audio_file = { 0 };
static AudioStream audio_stream = { 0 };
static int is_playing = 0;
static char* message = NULL;
static Vector2 message_sz = { 0 };
static float volume = 1.0;
static double volume_draw_t = 0.0;

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
        memset(fft_output, 0, FFTSIZE * sizeof(cmx));
        max = 0.0;
        min = 0.0;
    }
    is_playing = 0;
    if (IsPathFile(path)) {
        if (message) {
            free(message);
            message = NULL;
        }
        if (!load_wav_file(path, &audio_file)) {
            message = wav_get_error();
            TraceLog(LOG_ERROR, "Error while trying to load wav file:\n", message);
            TraceLog(LOG_ERROR, "%s\n", message);
            return 0;
        } else {
            const char* name = GetFileNameWithoutExt(path);
            message = calloc(strlen(name) + 1, sizeof(char));
            strcpy(message, name);
        }
        if (audio_file.formatty != 1) {
            const char* msg = "unsupported format (compression)";
            message = calloc(strlen(msg) + 1, sizeof(char));
            strcpy(message, msg);
        }
        message_sz = MeasureTextEx(GetFontDefault(), message, 24, 10);
    }
    audio_stream = LoadAudioStream(audio_file.samplert, audio_file.bits_per_sample, audio_file.nchan);
    SetAudioStreamCallback(audio_stream, audio_input_callback);
    SetAudioStreamVolume(audio_stream, volume);
    PlayAudioStream(audio_stream);
    is_playing = 1;

    TraceLog(LOG_INFO, "[WAV FILE]\n"
                       "- File size: %d bytes\n"
                       "- Format type: %d\n"
                       "- Number of channels: %d\n"
                       "- Sample rate: %d Hz\n"
                       "- Bytes per second: %d\n"
                       "- Bytes per block: %d\n"
                       "- Bits per sample: %d\n"
                       "- WAV data size: %d bytes\n"
                       "- Audio length: %02ld:%02ld",
        audio_file.fsize,
        audio_file.formatty,
        audio_file.nchan,
        audio_file.samplert,
        audio_file.bytes_per_sec,
        audio_file.bytes_per_block,
        audio_file.bits_per_sample,
        audio_file.datasz,
        audio_file.audio_time / 60,
        audio_file.audio_time % 60);
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
void update_volume(void)
{
    Vector2 wheel = GetMouseWheelMoveV();
    if (wheel.y == 0.0)
        return;
    volume += wheel.y * .1f;
    volume = Clamp(volume, 0.0, 1.0);
    SetAudioStreamVolume(audio_stream, volume);
    volume_draw_t = GetTime() + 3.;
}

void update_progress_bar(void)
{
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        return;
    const size_t whole = audio_file.datasz;
    const float h = PROGRESS_BAR_H * w_height;
    Rectangle r = { .x = 0.0, .y = w_height - h, .width = w_width, .height = h };
    Vector2 mouse = GetMousePosition();
    if (!CheckCollisionPointRec(mouse, r))
        return;
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
        message_sz = MeasureTextEx(GetFontDefault(), message, 24, 10);
    }
    if (IsFileDropped()) {
        FilePathList pl = LoadDroppedFiles();
        try_load_wav(pl.paths[0]);
        UnloadDroppedFiles(pl);
    }
    if (!last_frames || !cmx_pre || !is_playing)
        return;
    update_progress_bar();
    update_volume();

    size_t sample_rate = audio_file.bytes_per_block * 2;
    for (size_t i = 0; i < FFTSIZE; i++) {
        double v = 0;
        long int left = 0;
        long int right = 0;
        uint16_t nchan = audio_file.nchan;
        size_t offset = last_data_ptr + (i * audio_file.bytes_per_block * sample_rate);
        char* dp = audio_file.data + offset;
        if (offset >= audio_file.datasz)
            goto skip_sampling;
        switch (audio_file.bytes_per_block) {
        case 2: {
            unsigned char* left_p = (unsigned char*)(dp);
            left = *left_p;
            right = (nchan == 2) ? *(left_p + 1) : left;
        } break;
        case 4: {
            short* left_p = (short*)(dp);
            left = *left_p;
            right = (nchan == 2) ? *(left_p + 1) : left;
        } break;
        case 8: {
            int* left_p = (int*)(dp);
            left = *left_p;
            right = (nchan == 2) ? *(left_p + 1) : left;
        } break;
        }
        v = ((double)left + (double)right) * 0.5f / (1000.f);
    skip_sampling:
        fft_input[i] = cmx_re(v);
    }
    (void)cmx_fft2(fft_input, FFTSIZE, 0, cmx_pre, fft_output);
    max = 0.0;
    // https://stackoverflow.com/a/20584591
    for (size_t i = 0; i < FFTSIZE / 2; i += GROUPING_FACTOR) {
        double v = 0.0;
        for (size_t j = 0; j < GROUPING_FACTOR; j++) {
            double tmp = cmx_mod(fft_output[i + j]);
            tmp = SMOOTHING * fft_draw_data[i / GROUPING_FACTOR] + ((1 - SMOOTHING) * tmp);
            // tmp = 10 * log10(pow(tmp, 2));
            // Hamming window It doesn't really look that good tbh
            // const double a_0 = 0.54;
            // tmp *= a_0 - (1 - a_0) * cos((2 * M_PI * (j + i)) / (FFTSIZE / 2.));
            v += tmp;
        }
        v /= GROUPING_FACTOR;
        max = v > max ? v : max;
        min = v < min ? v : min;
        fft_draw_data[i / GROUPING_FACTOR] = v;
    }
    max *= 1.02;
}
void draw_volume(void)
{
    if (GetTime() > volume_draw_t)
        return;
    static char buf[32] = { 0 };
    sprintf(buf, "vol: %.1f", volume);
    DrawText(buf, 0, 0, 30, WHITE);
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
    static char time[32] = { 0 };
    const size_t elapsed = (wav_data_ptr / audio_file.bytes_per_sec);
    sprintf(time, "%02ld:%02ld",
        elapsed / 60,
        elapsed % 60);
    Vector2 sz = MeasureTextEx(GetFontDefault(), time, 16, 10);
    DrawTextEx(GetFontDefault(),
        time,
        (Vector2) { .x = (w_width / 2.) - (sz.x / 2.), .y = (w_height - h + h / 2.0) - (sz.y / 2.) },
        16,
        10,
        GRAY);
}
void draw_message(void)
{
    Vector2 mpos = {
        .x = w_width / 2.,
        .y = w_height / 2.
    };
    mpos = Vector2Subtract(mpos, Vector2Scale(message_sz, .5));
    DrawTextEx(GetFontDefault(), message, mpos, 24, 10, WHITE);
}
void draw(void)
{
    if (is_playing) {
        const float col_width = (float)w_width / (int)COLUMN_COUNT;
        const size_t whole = COLUMN_COUNT;
        const double total_height = (double)(w_height - pb_height);
        for (size_t i = 0; i < whole; i++) {
            double c = fft_draw_data[i];
            double v = (c / max) * total_height;
            Rectangle r = {
                .x = col_width * i,
                .y = total_height - v,
                .width = col_width,
                .height = v,
            };
            DrawRectangleRec(r, ColorFromHSV(((float)i / (float)whole) * 360., 1.0, 1.0));
        }
        draw_progress_bar();
        draw_volume();
    }
    if (message)
        draw_message();
}

int main(int argc, char** args)
{
    cmx_pre = cmx_precomp_reversed_bits(FFTSIZE);
    fft_input = calloc(FFTSIZE, sizeof(cmx));
    fft_output = calloc(FFTSIZE, sizeof(cmx));
    fft_draw_data = calloc(COLUMN_COUNT, sizeof(double));

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
    UnloadAudioStream(audio_stream);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
