#include <assert.h>
#include <string.h>

#include "wav_encoder.h"

static unsigned int read_u16_le(const uint8_t *buffer, size_t offset)
{
    return (unsigned int) buffer[offset] | ((unsigned int) buffer[offset + 1U] << 8U);
}

static unsigned int read_u32_le(const uint8_t *buffer, size_t offset)
{
    return (unsigned int) buffer[offset] |
           ((unsigned int) buffer[offset + 1U] << 8U) |
           ((unsigned int) buffer[offset + 2U] << 16U) |
           ((unsigned int) buffer[offset + 3U] << 24U);
}

int main(void)
{
    uint8_t header[WAV_ENCODER_HEADER_SIZE];
    uint8_t output[WAV_ENCODER_HEADER_SIZE + 160U];
    uint8_t pcm[160U];
    const size_t pcm_size = 160U;

    memset(pcm, 0x7F, sizeof(pcm));
    wav_encoder_write_header(header, pcm_size, 16000U, 1U, 16U);

    assert(memcmp(header, "RIFF", 4U) == 0);
    assert(memcmp(header + 8U, "WAVE", 4U) == 0);
    assert(memcmp(header + 12U, "fmt ", 4U) == 0);
    assert(memcmp(header + 36U, "data", 4U) == 0);
    assert(read_u32_le(header, 24U) == 16000U);
    assert(read_u16_le(header, 34U) == 16U);
    assert(read_u32_le(header, 40U) == pcm_size);
    assert(wav_encoder_total_size(pcm_size) == pcm_size + WAV_ENCODER_HEADER_SIZE);
    assert(wav_encoder_encode_pcm(output, sizeof(output), pcm, sizeof(pcm), 16000U, 1U, 16U, NULL));
    assert(memcmp(output, "RIFF", 4U) == 0);
    assert(memcmp(output + WAV_ENCODER_HEADER_SIZE, pcm, sizeof(pcm)) == 0);

    return 0;
}
