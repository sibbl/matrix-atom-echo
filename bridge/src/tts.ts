import type { ElevenLabsConfig } from "./config.js";
import { createToneAudioPayload } from "./audio.js";
import type { AudioPayload, TextToSpeechClient } from "./replies.js";

export class LocalToneTextToSpeechClient implements TextToSpeechClient {
  async synthesize(text: string): Promise<AudioPayload> {
    return createToneAudioPayload(text);
  }
}

export class ElevenLabsTextToSpeechClient implements TextToSpeechClient {
  constructor(private readonly config: ElevenLabsConfig) {}

  async synthesize(text: string): Promise<AudioPayload> {
    const response = await fetch(
      `https://api.elevenlabs.io/v1/text-to-speech/${encodeURIComponent(this.config.voiceId)}`,
      {
        method: "POST",
        headers: {
          Accept: "audio/mpeg",
          "Content-Type": "application/json",
          "xi-api-key": this.config.apiKey
        },
        body: JSON.stringify({
          text,
          model_id: this.config.modelId,
          output_format: this.config.outputFormat
        })
      }
    );

    if (!response.ok) {
      throw new Error(`ElevenLabs TTS failed with status ${response.status}`);
    }

    const arrayBuffer = await response.arrayBuffer();
    return {
      contentType: response.headers.get("content-type") ?? "audio/mpeg",
      buffer: Buffer.from(arrayBuffer),
      createdAt: Date.now()
    };
  }
}
