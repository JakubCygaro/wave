#include "wavfile.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char _wav_error[128] = { 0 };

char* wav_get_error(){
    if(_wav_error[0] == '\0') return NULL;
    char* ret = calloc(strlen(_wav_error) + 1, sizeof(char));
    strcpy(ret, _wav_error);
    memset(_wav_error, 0, sizeof(_wav_error) / sizeof(char));
    return ret;
}

void wavefile_free(WaveFile wav)
{
    if (wav.data) {
        free(wav.data);
    }
}

static int pass_until(const char* pat, FILE* file)
{
    int c = 0;
    char* bufp = (char*)pat;
    while (1) {
        c = fgetc(file);
        if (c == EOF) {
            sprintf(_wav_error, "Failed reading header (premature end while looking for fmt data)\n");
            return 0;
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
    return 1;
}

int load_wav_file(const char* path, WaveFile* wav)
{
    *wav = (WaveFile){ 0 };
    FILE* f = fopen(path, "r");
    if (!f) {
        sprintf(_wav_error, "Could not open file `%s` for reading\n", path);
        return 0;
    }
    static char buf[64] = { 0 };
    if (fread(buf, 1, 4, f) != 4 || !streq(buf, "RIFF")) {
        sprintf(_wav_error, "Failed reading header (RIFF)\n");
        return 0;
    }

    if (fread(buf, 1, 4, f) != 4) {
        sprintf(_wav_error, "Failed reading header (file size)\n");
        return 0;
    }
    wav->fsize = *((uint32_t*)buf);

    if (fread(buf, 1, 4, f) != 4 || !streq(buf, "WAVE")) {
        sprintf(_wav_error, "Failed reading header (WAVE)\n");
        return 0;
    }
    memset(buf, 0, sizeof(buf));
    pass_until("fmt\x20", f);

    uint32_t fmtlen = 0;
    if (fread(buf, 1, 4, f) != 4) {
        sprintf(_wav_error, "Failed reading header (length of format data)\n");
        return 0;
    }
    fmtlen = *((uint32_t*)buf);
    (void)fmtlen;

    if (fread(buf, 1, 2, f) != 2) {
        sprintf(_wav_error, "Failed reading header (type of format)\n");
        return 0;
    }
    wav->formatty = *((uint16_t*)buf);

    if (fread(buf, 1, 2, f) != 2) {
        sprintf(_wav_error, "Failed reading header (number of channels)\n");
        return 0;
    }
    wav->nchan = *((uint16_t*)buf);

    if (fread(buf, 1, 4, f) != 4) {
        sprintf(_wav_error, "Failed reading header (sample rate)\n");
        return 0;
    }
    wav->samplert = *((uint32_t*)buf);

    if (fread(buf, 1, 4, f) != 4) {
        sprintf(_wav_error, "Failed reading header (bytes per second)\n");
        return 0;
    }
    wav->bytes_per_sec = *((uint32_t*)buf);

    if (fread(buf, 1, 2, f) != 2) {
        sprintf(_wav_error, "Failed reading header (bytes per block)\n");
        return 0;
    }
    wav->bytes_per_block = *((uint16_t*)buf);

    if (fread(buf, 1, 2, f) != 2) {
        sprintf(_wav_error, "Failed reading header (bits per sample)\n");
        return 0;
    }
    wav->bits_per_sample = *((uint16_t*)buf);

    memset(buf, 0, sizeof(buf));
    pass_until("data", f);

    if (fread(buf, 1, 4, f) != 4) {
        sprintf(_wav_error, "Failed reading header (data size)\n");
        return 0;
    }
    wav->datasz = *((uint32_t*)buf);

    wav->data = calloc(wav->datasz, 1);

    int b = 0;
    for (size_t i = 1; i <= wav->datasz; i++) {
        b = fgetc(f);
        if (b == EOF) break;
        wav->data[i - 1] = (char)b;
    }
    wav->audio_time = wav->datasz / wav->bytes_per_sec;
    fclose(f);
    return 1;
}
