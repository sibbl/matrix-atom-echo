import { randomUUID } from "node:crypto";

import {
  getDeviceState,
  pushAudio,
  registerMatrixThreadRoot,
  type QueuedAudio
} from "./devices.js";

export interface AudioPayload {
  id?: string;
  contentType: string;
  buffer: Buffer;
  createdAt?: number;
}

export interface TextToSpeechClient {
  synthesize(text: string): Promise<AudioPayload>;
}

export interface AudioDownloadClient {
  downloadAudio(sourceUrl: string): Promise<AudioPayload>;
}

let generatedAudioCounter = 0;

function toQueuedAudio(source: QueuedAudio["source"], payload: AudioPayload): QueuedAudio {
  generatedAudioCounter += 1;

  return {
    id: payload.id ?? `audio-${generatedAudioCounter}-${randomUUID()}`,
    contentType: payload.contentType,
    buffer: payload.buffer,
    createdAt: payload.createdAt ?? Date.now(),
    source
  };
}

export function registerOutgoingMatrixMessage(
  deviceId: string,
  eventId: string
): { threadRootEventId: string; isNewThread: boolean } {
  const isNewThread = getDeviceState(deviceId).currentThreadEventId === undefined;
  const threadRootEventId = registerMatrixThreadRoot(deviceId, eventId);

  return {
    threadRootEventId,
    isNewThread
  };
}

export async function queueBotTextReply(
  deviceId: string,
  text: string,
  client: TextToSpeechClient
): Promise<QueuedAudio> {
  const payload = await client.synthesize(text);
  return pushAudio(deviceId, toQueuedAudio("tts", payload));
}

export async function queueBotAudioReply(
  deviceId: string,
  sourceUrl: string,
  client: AudioDownloadClient
): Promise<QueuedAudio> {
  const payload = await client.downloadAudio(sourceUrl);
  return pushAudio(deviceId, toQueuedAudio("matrix-audio", payload));
}
