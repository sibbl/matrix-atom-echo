import { createServer, type IncomingMessage, type Server, type ServerResponse } from "node:http";

import {
  isBasicAuthAuthorized,
  isDeviceAuthorized,
  readBasicAuthCredentials,
  readDeviceToken
} from "./auth.js";
import { BridgeService, type BridgeDependencies } from "./bridge-service.js";
import { bridgeConfigFromEnv, type BridgeConfig } from "./config.js";
import { FetchAudioDownloadClient } from "./download.js";
import { MatrixApiClient } from "./matrix.js";
import { ElevenLabsTextToSpeechClient, LocalToneTextToSpeechClient } from "./tts.js";

export interface BridgeRuntime {
  config: BridgeConfig;
  service: BridgeService;
  server: Server;
}

function json(
  response: ServerResponse,
  statusCode: number,
  body: object,
  headers?: Record<string, string>
): void {
  const payload = JSON.stringify(body);
  response.writeHead(statusCode, {
    "Content-Length": Buffer.byteLength(payload),
    "Content-Type": "application/json",
    ...(headers ?? {})
  });
  response.end(payload);
}

async function readBody(request: IncomingMessage): Promise<Buffer> {
  const chunks: Buffer[] = [];
  for await (const chunk of request) {
    chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk));
  }

  return Buffer.concat(chunks);
}

async function readJsonBody(request: IncomingMessage): Promise<unknown> {
  const body = await readBody(request);
  if (body.length === 0) {
    return {};
  }

  try {
    return JSON.parse(body.toString("utf8")) as unknown;
  } catch {
    throw new Error("request body must be valid JSON");
  }
}

function ensureAuthorized(
  request: IncomingMessage,
  response: ServerResponse,
  config: BridgeConfig,
  options: {
    allowBasicAuth: boolean;
    allowDeviceToken: boolean;
  }
): boolean {
  const requiresDeviceToken = options.allowDeviceToken && Boolean(config.deviceAuthToken);
  const requiresBasicAuth = options.allowBasicAuth && Boolean(config.basicAuth);

  if (!requiresDeviceToken && !requiresBasicAuth) {
    return true;
  }

  const deviceAuthorized =
    requiresDeviceToken && isDeviceAuthorized(readDeviceToken(request.headers), config.deviceAuthToken);
  const basicAuthorized =
    requiresBasicAuth &&
    isBasicAuthAuthorized(readBasicAuthCredentials(request.headers), config.basicAuth);

  if (deviceAuthorized || basicAuthorized) {
    return true;
  }

  json(response, 401, {
    error: "Unauthorized"
  }, config.basicAuth ? { "WWW-Authenticate": 'Basic realm="matrix-atom-echo"' } : undefined);
  return false;
}

function createDependencies(config: BridgeConfig, overrides?: Partial<BridgeDependencies>): BridgeDependencies {
  const defaultMatrixClient = config.matrix ? new MatrixApiClient(config.matrix) : undefined;
  const matrixClient = overrides?.matrixClient ?? defaultMatrixClient;
  const ttsClient =
    overrides?.ttsClient ??
    (config.elevenLabs ? new ElevenLabsTextToSpeechClient(config.elevenLabs) : new LocalToneTextToSpeechClient());
  const audioDownloadClient =
    overrides?.audioDownloadClient ??
    (defaultMatrixClient ? defaultMatrixClient : new FetchAudioDownloadClient());

  return {
    replyMode: overrides?.replyMode ?? config.replyMode,
    matrixClient,
    ttsClient,
    audioDownloadClient,
    now: overrides?.now
  };
}

function asRecord(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error("request body must be a JSON object");
  }

  return value as Record<string, unknown>;
}

function readRequiredString(value: unknown, fieldName: string): string {
  if (typeof value !== "string" || !value.trim()) {
    throw new Error(`${fieldName} is required`);
  }

  return value.trim();
}

export function createBridgeRuntime(options?: {
  config?: BridgeConfig;
  dependencies?: Partial<BridgeDependencies>;
}): BridgeRuntime {
  const config = options?.config ?? bridgeConfigFromEnv();
  const service = new BridgeService(createDependencies(config, options?.dependencies));

  const server = createServer(async (request, response) => {
    try {
      const method = request.method ?? "GET";
      const url = new URL(request.url ?? "/", `http://${request.headers.host ?? "localhost"}`);

      if (url.pathname === "/health") {
        json(response, 200, {
          ok: true,
          bridgeBaseUrl: config.bridgeBaseUrl,
          replyMode: config.replyMode,
          matrixEnabled: Boolean(config.matrix),
          elevenLabsEnabled: Boolean(config.elevenLabs)
        });
        return;
      }

      const deviceRouteMatch = url.pathname.match(
        /^\/devices\/([^/]+)\/(recording|next-audio|replay-last-audio|thread\/reset|state)$/
      );

      if (deviceRouteMatch) {
        const deviceId = decodeURIComponent(deviceRouteMatch[1] ?? "");
        const route = deviceRouteMatch[2] ?? "";

        if (
          !ensureAuthorized(request, response, config, {
            allowBasicAuth: true,
            allowDeviceToken: true
          })
        ) {
          return;
        }

        if (route === "recording") {
          if (method !== "POST") {
            response.writeHead(405, { Allow: "POST" });
            response.end();
            return;
          }

          const body = await readBody(request);
          const result = await service.submitRecording(deviceId, request.headers["content-type"], body);
          json(response, 202, result);
          return;
        }

        if (route === "next-audio") {
          if (method !== "GET") {
            response.writeHead(405, { Allow: "GET" });
            response.end();
            return;
          }

          const nextAudio = service.popNextAudio(deviceId);
          if (!nextAudio) {
            response.writeHead(204);
            response.end();
            return;
          }

          response.writeHead(200, {
            "Content-Length": nextAudio.buffer.length,
            "Content-Type": nextAudio.contentType,
            "X-Audio-Id": nextAudio.id,
            "X-Queue-Depth": String(service.getDeviceSummary(deviceId).queueDepth)
          });
          response.end(nextAudio.buffer);
          return;
        }

        if (route === "replay-last-audio") {
          if (method !== "POST") {
            response.writeHead(405, { Allow: "POST" });
            response.end();
            return;
          }

          const replayed = service.replayLastAudio(deviceId);
          if (!replayed) {
            json(response, 404, { error: "No audio available for replay" });
            return;
          }

          json(response, 202, {
            queuedAudioId: replayed.id
          });
          return;
        }

        if (route === "thread/reset") {
          if (method !== "POST") {
            response.writeHead(405, { Allow: "POST" });
            response.end();
            return;
          }

          json(response, 202, service.resetDeviceThread(deviceId));
          return;
        }

        if (method !== "GET") {
          response.writeHead(405, { Allow: "GET" });
          response.end();
          return;
        }

        json(response, 200, service.getDeviceSummary(deviceId));
        return;
      }

      if (url.pathname === "/integrations/matrix/text-reply") {
        if (
          !ensureAuthorized(request, response, config, {
            allowBasicAuth: true,
            allowDeviceToken: false
          })
        ) {
          return;
        }

        if (method !== "POST") {
          response.writeHead(405, { Allow: "POST" });
          response.end();
          return;
        }

        const body = asRecord(await readJsonBody(request));
        const text = readRequiredString(body.text, "text");
        const queued =
          typeof body.deviceId === "string" && body.deviceId.trim()
            ? await service.queueTextReplyForDevice(body.deviceId.trim(), text)
            : await service.queueTextReplyForThread(
                readRequiredString(body.threadRootEventId, "threadRootEventId"),
                text
              );

        json(response, 202, {
          queuedAudioId: queued.id,
          contentType: queued.contentType
        });
        return;
      }

      if (url.pathname === "/integrations/matrix/audio-reply") {
        if (
          !ensureAuthorized(request, response, config, {
            allowBasicAuth: true,
            allowDeviceToken: false
          })
        ) {
          return;
        }

        if (method !== "POST") {
          response.writeHead(405, { Allow: "POST" });
          response.end();
          return;
        }

        const body = asRecord(await readJsonBody(request));
        const sourceUrl = readRequiredString(body.sourceUrl, "sourceUrl");
        const queued =
          typeof body.deviceId === "string" && body.deviceId.trim()
            ? await service.queueAudioReplyForDevice(body.deviceId.trim(), sourceUrl)
            : await service.queueAudioReplyForThread(
                readRequiredString(body.threadRootEventId, "threadRootEventId"),
                sourceUrl
              );

        json(response, 202, {
          queuedAudioId: queued.id,
          contentType: queued.contentType
        });
        return;
      }

      json(response, 404, {
        error: "Not found"
      });
    } catch (error) {
      const message = error instanceof Error ? error.message : "Unknown error";
      json(response, 400, {
        error: message
      });
    }
  });

  return {
    config,
    service,
    server
  };
}
