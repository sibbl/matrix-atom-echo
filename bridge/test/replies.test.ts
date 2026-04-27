import { beforeEach, describe, expect, it } from "vitest";

import { BridgeService } from "../src/bridge-service.js";
import { getDeviceState, resetAllDeviceState } from "../src/devices.js";
import {
  queueBotAudioReply,
  queueBotTextReply,
  registerOutgoingMatrixMessage
} from "../src/replies.js";

describe("matrix thread roots", () => {
  beforeEach(() => {
    resetAllDeviceState();
  });

  it("creates a thread root on the first matrix send", () => {
    const result = registerOutgoingMatrixMessage("default", "$root");

    expect(result).toEqual({
      threadRootEventId: "$root",
      isNewThread: true
    });
  });

  it("reuses the existing thread root on later sends", () => {
    registerOutgoingMatrixMessage("default", "$root");

    const result = registerOutgoingMatrixMessage("default", "$second");

    expect(result).toEqual({
      threadRootEventId: "$root",
      isNewThread: false
    });
  });
});

describe("reply queueing", () => {
  beforeEach(() => {
    resetAllDeviceState();
  });

  it("queues synthesized audio for a bot text reply", async () => {
    const queued = await queueBotTextReply("text-reply", "hello", {
      async synthesize(text) {
        expect(text).toBe("hello");

        return {
          id: "tts-1",
          contentType: "audio/mpeg",
          buffer: Buffer.from("mp3"),
          createdAt: 10
        };
      }
    });

    const state = getDeviceState("text-reply");

    expect(queued.id).toBe("tts-1");
    expect(queued.source).toBe("tts");
    expect(state.queue).toHaveLength(1);
  });

  it("queues downloaded audio for a bot audio reply", async () => {
    const queued = await queueBotAudioReply("audio-reply", "mxc://example/audio", {
      async downloadAudio(sourceUrl) {
        expect(sourceUrl).toBe("mxc://example/audio");

        return {
          id: "audio-1",
          contentType: "audio/wav",
          buffer: Buffer.from("wav"),
          createdAt: 20
        };
      }
    });

    const state = getDeviceState("audio-reply");

    expect(queued.id).toBe("audio-1");
    expect(queued.source).toBe("matrix-audio");
    expect(state.queue).toHaveLength(1);
  });
});

describe("bridge service", () => {
  beforeEach(() => {
    resetAllDeviceState();
  });

  it("reuses the first matrix event as the thread root for later recordings", async () => {
    const sendCalls: Array<string | undefined> = [];
    const service = new BridgeService({
      replyMode: "matrix",
      ttsClient: {
        synthesize: async () => ({
          contentType: "audio/wav",
          buffer: Buffer.alloc(0)
        })
      },
      audioDownloadClient: {
        downloadAudio: async () => ({
          contentType: "audio/wav",
          buffer: Buffer.alloc(0)
        })
      },
      matrixClient: {
        async sendDeviceRecording({ threadRootEventId }) {
          sendCalls.push(threadRootEventId);
          return {
            eventId: threadRootEventId ? "$follow-up" : "$root"
          };
        }
      }
    });

    const first = await service.submitRecording("matrix-device", "audio/wav", Buffer.from("one"));
    const second = await service.submitRecording("matrix-device", "audio/wav", Buffer.from("two"));

    expect(first).toMatchObject({
      threadRootEventId: "$root",
      isNewThread: true
    });
    expect(second).toMatchObject({
      threadRootEventId: "$root",
      isNewThread: false
    });
    expect(sendCalls).toEqual([undefined, "$root"]);
  });
});
