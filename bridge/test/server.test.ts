import type { AddressInfo } from "node:net";

import { afterEach, beforeEach, describe, expect, it } from "vitest";

import { createBridgeRuntime } from "../src/server.js";
import { parseBridgeConfig } from "../src/config.js";
import { resetAllDeviceState } from "../src/devices.js";

async function startTestServer(options?: Parameters<typeof createBridgeRuntime>[0]) {
  const runtime = createBridgeRuntime({
    config:
      options?.config ??
      parseBridgeConfig({
        bridgeHost: "127.0.0.1",
        bridgePort: 3000,
        replyMode: "echo"
      }),
    dependencies: options?.dependencies
  });

  await new Promise<void>((resolve) => {
    runtime.server.listen(0, "127.0.0.1", resolve);
  });

  const address = runtime.server.address() as AddressInfo;

  return {
    runtime,
    baseUrl: `http://127.0.0.1:${address.port}`,
    async close() {
      await new Promise<void>((resolve, reject) => {
        runtime.server.close((error) => {
          if (error) {
            reject(error);
            return;
          }

          resolve();
        });
      });
    }
  };
}

describe("bridge HTTP server", () => {
  const servers: Array<Awaited<ReturnType<typeof startTestServer>>> = [];

  beforeEach(() => {
    resetAllDeviceState();
  });

  afterEach(async () => {
    while (servers.length > 0) {
      await servers.pop()?.close();
    }
  });

  it("reports health", async () => {
    const server = await startTestServer();
    servers.push(server);

    const response = await fetch(`${server.baseUrl}/health`);

    expect(response.status).toBe(200);
    await expect(response.json()).resolves.toMatchObject({
      ok: true,
      replyMode: "echo"
    });
  });

  it("accepts recordings, queues echo audio, and drains next-audio", async () => {
    const server = await startTestServer();
    servers.push(server);
    const payload = Buffer.from("RIFFdemo");

    const recordResponse = await fetch(`${server.baseUrl}/devices/default/recording`, {
      method: "POST",
      headers: {
        "Content-Type": "audio/wav"
      },
      body: payload
    });

    expect(recordResponse.status).toBe(202);
    await expect(recordResponse.json()).resolves.toMatchObject({
      accepted: true,
      queuedAudioId: expect.stringContaining("echo-")
    });

    const firstAudioResponse = await fetch(`${server.baseUrl}/devices/default/next-audio`);
    expect(firstAudioResponse.status).toBe(200);
    expect(firstAudioResponse.headers.get("content-type")).toBe("audio/wav");
    expect(Buffer.from(await firstAudioResponse.arrayBuffer())).toEqual(payload);

    const emptyResponse = await fetch(`${server.baseUrl}/devices/default/next-audio`);
    expect(emptyResponse.status).toBe(204);
  });

  it("enforces optional device auth on device endpoints", async () => {
    const server = await startTestServer({
      config: parseBridgeConfig({
        bridgeHost: "127.0.0.1",
        bridgePort: 3000,
        replyMode: "echo",
        deviceAuthToken: "shared-secret"
      })
    });
    servers.push(server);

    const unauthorized = await fetch(`${server.baseUrl}/devices/default/recording`, {
      method: "POST",
      headers: {
        "Content-Type": "audio/wav"
      },
      body: Buffer.from("wav")
    });
    expect(unauthorized.status).toBe(401);

    const authorized = await fetch(`${server.baseUrl}/devices/default/recording`, {
      method: "POST",
      headers: {
        "Content-Type": "audio/wav",
        Authorization: "Bearer shared-secret"
      },
      body: Buffer.from("wav")
    });
    expect(authorized.status).toBe(202);
  });

  it("accepts optional basic auth on device endpoints", async () => {
    const server = await startTestServer({
      config: parseBridgeConfig({
        bridgeHost: "127.0.0.1",
        bridgePort: 3000,
        replyMode: "echo",
        basicAuth: {
          username: "admin",
          password: "s3cret"
        }
      })
    });
    servers.push(server);

    const unauthorized = await fetch(`${server.baseUrl}/devices/default/recording`, {
      method: "POST",
      headers: {
        "Content-Type": "audio/wav"
      },
      body: Buffer.from("wav")
    });
    expect(unauthorized.status).toBe(401);
    expect(unauthorized.headers.get("www-authenticate")).toContain("Basic");

    const basicHeader = Buffer.from("admin:s3cret", "utf8").toString("base64");
    const authorized = await fetch(`${server.baseUrl}/devices/default/recording`, {
      method: "POST",
      headers: {
        "Content-Type": "audio/wav",
        Authorization: `Basic ${basicHeader}`
      },
      body: Buffer.from("wav")
    });
    expect(authorized.status).toBe(202);
  });

  it("replays the last audio and resets thread state", async () => {
    const server = await startTestServer();
    servers.push(server);

    await fetch(`${server.baseUrl}/devices/default/recording`, {
      method: "POST",
      headers: {
        "Content-Type": "audio/wav"
      },
      body: Buffer.from("audio")
    });
    await fetch(`${server.baseUrl}/devices/default/next-audio`);

    const replayResponse = await fetch(`${server.baseUrl}/devices/default/replay-last-audio`, {
      method: "POST"
    });
    expect(replayResponse.status).toBe(202);

    const replayedAudio = await fetch(`${server.baseUrl}/devices/default/next-audio`);
    expect(replayedAudio.status).toBe(200);
    expect(replayedAudio.headers.get("x-audio-id")).toContain("-replay-");

    const resetResponse = await fetch(`${server.baseUrl}/devices/default/thread/reset`, {
      method: "POST"
    });
    expect(resetResponse.status).toBe(202);
    const resetResult = (await resetResponse.json()) as {
      currentThreadEventId?: string;
      lastIncomingEventId?: string;
    };
    expect(resetResult.currentThreadEventId).toBeUndefined();
    expect(resetResult.lastIncomingEventId).toBeUndefined();
  });

  it("queues text and audio replies through the integration endpoints", async () => {
    const server = await startTestServer({
      dependencies: {
        ttsClient: {
          async synthesize(text) {
            return {
              id: `tts-${text}`,
              contentType: "audio/wav",
              buffer: Buffer.from(`tts:${text}`),
              createdAt: 1
            };
          }
        },
        audioDownloadClient: {
          async downloadAudio(sourceUrl) {
            return {
              id: `audio-${sourceUrl}`,
              contentType: "audio/wav",
              buffer: Buffer.from(`audio:${sourceUrl}`),
              createdAt: 2
            };
          }
        }
      }
    });
    servers.push(server);

    const recordingResponse = await fetch(`${server.baseUrl}/devices/device-1/recording`, {
      method: "POST",
      headers: {
        "Content-Type": "audio/wav"
      },
      body: Buffer.from("wav")
    });
    const recordingResult = (await recordingResponse.json()) as { threadRootEventId: string };

    const textReply = await fetch(`${server.baseUrl}/integrations/matrix/text-reply`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json"
      },
      body: JSON.stringify({
        threadRootEventId: recordingResult.threadRootEventId,
        text: "hello"
      })
    });
    expect(textReply.status).toBe(202);

    const audioReply = await fetch(`${server.baseUrl}/integrations/matrix/audio-reply`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json"
      },
      body: JSON.stringify({
        deviceId: "device-1",
        sourceUrl: "https://example.com/audio.wav"
      })
    });
    expect(audioReply.status).toBe(202);

    const nextAudio = await fetch(`${server.baseUrl}/devices/device-1/next-audio`);
    expect(nextAudio.status).toBe(200);
    expect(Buffer.from(await nextAudio.arrayBuffer()).toString("utf8")).toBe("wav");

    const nextQueued = await fetch(`${server.baseUrl}/devices/device-1/next-audio`);
    expect(nextQueued.status).toBe(200);
    expect(Buffer.from(await nextQueued.arrayBuffer()).toString("utf8")).toBe("tts:hello");

    const finalQueued = await fetch(`${server.baseUrl}/devices/device-1/next-audio`);
    expect(finalQueued.status).toBe(200);
    expect(Buffer.from(await finalQueued.arrayBuffer()).toString("utf8")).toBe(
      "audio:https://example.com/audio.wav"
    );
  });

  it("requires basic auth on integration endpoints when configured", async () => {
    const server = await startTestServer({
      config: parseBridgeConfig({
        bridgeHost: "127.0.0.1",
        bridgePort: 3000,
        replyMode: "echo",
        basicAuth: {
          username: "admin",
          password: "s3cret"
        }
      })
    });
    servers.push(server);

    const unauthorized = await fetch(`${server.baseUrl}/integrations/matrix/text-reply`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json"
      },
      body: JSON.stringify({
        deviceId: "device-1",
        text: "hello"
      })
    });
    expect(unauthorized.status).toBe(401);

    const basicHeader = Buffer.from("admin:s3cret", "utf8").toString("base64");
    const authorized = await fetch(`${server.baseUrl}/integrations/matrix/text-reply`, {
      method: "POST",
      headers: {
        Authorization: `Basic ${basicHeader}`,
        "Content-Type": "application/json"
      },
      body: JSON.stringify({
        deviceId: "device-1",
        text: "hello"
      })
    });
    expect(authorized.status).toBe(202);
  });
});
