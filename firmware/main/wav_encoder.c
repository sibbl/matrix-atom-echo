#include "wav_encoder.h"

#include <string.h>

static void write_u16_le(uint8_t *buffer, size_t offset, uint16_t value)
{
    buffer[offset] = (uint8_t) (value & 0xFFU);
    buffer[offset + 1U] = (uint8_t) ((value >> 8U) & 0xFFU);
}

static void write_u32_le(uint8_t *buffer, size_t offset, uint32_t value)
{
    buffer[offset] = (uint8_t) (value & 0xFFU);
    buffer[offset + 1U] = (uint8_t) ((value >> 8U) & 0xFFU);
    buffer[offset + 2U] = (uint8_t) ((value >> 16U) & 0xFFU);
    buffer[offset + 3U] = (uint8_t) ((value >> 24U) & 0xFFU);
}

void wav_encoder_write_header(
    uint8_t header[WAV_ENCODER_HEADER_SIZE],
    size_t pcm_size_bytes,
    uint32_t sample_rate_hz,
    uint16_t channel_count,
    uint16_t bits_per_sample)
{
    const uint32_t byte_rate = sample_rate_hz * channel_count * bits_per_sample / 8U;
    const uint16_t block_align = (uint16_t) (channel_count * bits_per_sample / 8U);

    memset(header, 0, WAV_ENCODER_HEADER_SIZE);
    memcpy(header, "RIFF", 4U);
    write_u32_le(header, 4U, (uint32_t) (pcm_size_bytes + WAV_ENCODER_HEADER_SIZE - 8U));
    memcpy(header + 8U, "WAVE", 4U);
    memcpy(header + 12U, "fmt ", 4U);
    write_u32_le(header, 16U, 16U);
    write_u16_le(header, 20U, 1U);
    write_u16_le(header, 22U, channel_count);
    write_u32_le(header, 24U, sample_rate_hz);
    write_u32_le(header, 28U, byte_rate);
    write_u16_le(header, 32U, block_align);
    write_u16_le(header, 34U, bits_per_sample);
    memcpy(header + 36U, "data", 4U);
    write_u32_le(header, 40U, (uint32_t) pcm_size_bytes);
}

size_t wav_encoder_total_size(size_t pcm_size_bytes)
{
    return WAV_ENCODER_HEADER_SIZE + pcm_size_bytes;
}

bool wav_encoder_encode_pcm(
    uint8_t *output_buffer,
    size_t output_buffer_size,
    const uint8_t *pcm_data,
    size_t pcm_size_bytes,
    uint32_t sample_rate_hz,
    uint16_t channel_count,
    uint16_t bits_per_sample,
    size_t *written_size_bytes)
{
    const size_t total_size_bytes = wav_encoder_total_size(pcm_size_bytes);

    if (output_buffer == NULL || pcm_data == NULL) {
        return false;
    }

    if (output_buffer_size < total_size_bytes) {
        return false;
    }

    wav_encoder_write_header(
        output_buffer,
        pcm_size_bytes,
        sample_rate_hz,
        channel_count,
        bits_per_sample);
    memcpy(output_buffer + WAV_ENCODER_HEADER_SIZE, pcm_data, pcm_size_bytes);

    if (written_size_bytes != NULL) {
        *written_size_bytes = total_size_bytes;
    }

    return true;
}
