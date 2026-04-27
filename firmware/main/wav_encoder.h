#ifndef WAV_ENCODER_H
#define WAV_ENCODER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WAV_ENCODER_HEADER_SIZE 44U

void wav_encoder_write_header(
    uint8_t header[WAV_ENCODER_HEADER_SIZE],
    size_t pcm_size_bytes,
    uint32_t sample_rate_hz,
    uint16_t channel_count,
    uint16_t bits_per_sample);

size_t wav_encoder_total_size(size_t pcm_size_bytes);

bool wav_encoder_encode_pcm(
    uint8_t *output_buffer,
    size_t output_buffer_size,
    const uint8_t *pcm_data,
    size_t pcm_size_bytes,
    uint32_t sample_rate_hz,
    uint16_t channel_count,
    uint16_t bits_per_sample,
    size_t *written_size_bytes);

#endif
