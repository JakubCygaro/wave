#ifndef WAVFILE_H
#define WAVFILE_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define streq(A, B) (strcmp(A, B) == 0)
void wavefile_free(WaveFile wav);
int load_wav_file(const char* path, WaveFile* ret);
char* wav_get_error(void);

#endif
