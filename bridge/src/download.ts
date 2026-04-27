import { randomUUID } from "node:crypto";

import type { AudioDownloadClient, AudioPayload } from "./replies.js";

export class FetchAudioDownloadClient implements AudioDownloadClient {
  async downloadAudio(sourceUrl: string): Promise<AudioPayload> {
    const url = new URL(sourceUrl);
    if (!["http:", "https:"].includes(url.protocol)) {
      throw new Error("downloadAudio only supports http(s) URLs without Matrix config");
    }

    const response = await fetch(url);
    if (!response.ok) {
      throw new Error(`audio download failed with status ${response.status}`);
    }

    const contentType = response.headers.get("content-type") ?? "application/octet-stream";
    const arrayBuffer = await response.arrayBuffer();

    return {
      id: `download-${randomUUID()}`,
      contentType,
      buffer: Buffer.from(arrayBuffer),
      createdAt: Date.now()
    };
  }
}
