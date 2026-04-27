import { randomUUID } from "node:crypto";

import type { AudioPayload } from "./replies.js";

export interface WaveFormat {
  channelCount?: number;
  bitsPerSample?: number;
  sampleRateHz?: number;
}

export function assertAudioPayload(contentType: string | undefined, buffer: Buffer): void {
  if (!contentType?.startsWith("audio/")) {
    throw new Error("recordings must use an audio/* content type");
  }

  if (buffer.length === 0) {
    throw new Error("recordings must not be empty");
  }
}

export function createWavBuffer(
  pcmData: Buffer,
  format: WaveFormat = {}
): Buffer {
  const channelCount = format.channelCount ?? 1;
  const bitsPerSample = format.bitsPerSample ?? 16;
  const sampleRateHz = format.sampleRateHz ?? 16_000;
  const header = Buffer.alloc(44);
  const blockAlign = (channelCount * bitsPerSample) / 8;
  const byteRate = sampleRateHz * blockAlign;

  header.write("RIFF", 0, "ascii");
  header.writeUInt32LE(36 + pcmData.length, 4);
  header.write("WAVE", 8, "ascii");
  header.write("fmt ", 12, "ascii");
  header.writeUInt32LE(16, 16);
  header.writeUInt16LE(1, 20);
  header.writeUInt16LE(channelCount, 22);
  header.writeUInt32LE(sampleRateHz, 24);
  header.writeUInt32LE(byteRate, 28);
  header.writeUInt16LE(blockAlign, 32);
  header.writeUInt16LE(bitsPerSample, 34);
  header.write("data", 36, "ascii");
  header.writeUInt32LE(pcmData.length, 40);

  return Buffer.concat([header, pcmData]);
}

export function createToneAudioPayload(text: string): AudioPayload {
  const sampleRateHz = 16_000;
  const durationMs = Math.max(350, Math.min(2_000, text.length * 55 || 500));
  const totalSamples = Math.max(1, Math.floor((sampleRateHz * durationMs) / 1000));
  const pcm = Buffer.alloc(totalSamples * 2);
  const frequencyHz = 440 + (text.length % 5) * 110;
  const halfPeriodSamples = Math.max(1, Math.floor(sampleRateHz / (frequencyHz * 2)));
  const amplitude = 9_000;

  for (let sampleIndex = 0; sampleIndex < totalSamples; sampleIndex += 1) {
    const high = Math.floor(sampleIndex / halfPeriodSamples) % 2 === 0;
    pcm.writeInt16LE(high ? amplitude : -amplitude, sampleIndex * 2);
  }

  return {
    id: `tone-${randomUUID()}`,
    contentType: "audio/wav",
    buffer: createWavBuffer(pcm, { sampleRateHz }),
    createdAt: Date.now()
  };
}
