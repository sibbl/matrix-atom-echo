import type { BasicAuthConfig } from "./auth.js";

export type ReplyMode = "echo" | "matrix" | "none";

export interface RawBasicAuthConfig {
  username?: string;
  password?: string;
}

export interface RawMatrixConfig {
  homeserverUrl?: string;
  accessToken?: string;
  roomId?: string;
  userId?: string;
  botUserId?: string;
}

export interface MatrixConfig {
  homeserverUrl: string;
  accessToken: string;
  roomId: string;
  userId?: string;
  botUserId?: string;
}

export interface RawElevenLabsConfig {
  apiKey?: string;
  voiceId?: string;
  modelId?: string;
  outputFormat?: string;
}

export interface ElevenLabsConfig {
  apiKey: string;
  voiceId: string;
  modelId: string;
  outputFormat: string;
}

export interface RawBridgeConfig {
  bridgeHost?: string;
  bridgeBaseUrl?: string;
  bridgePort?: number | string;
  deviceAuthToken?: string | null;
  basicAuth?: RawBasicAuthConfig;
  replyMode?: string;
  matrix?: RawMatrixConfig;
  elevenLabs?: RawElevenLabsConfig;
}

export interface BridgeConfig {
  bridgeHost: string;
  bridgeBaseUrl: string;
  bridgePort: number;
  deviceAuthToken?: string;
  basicAuth?: BasicAuthConfig;
  replyMode: ReplyMode;
  matrix?: MatrixConfig;
  elevenLabs?: ElevenLabsConfig;
}

function parsePort(port: number | string | undefined): number {
  if (port === undefined) {
    return 3000;
  }

  const parsed = typeof port === "number" ? port : Number(port);
  if (!Number.isInteger(parsed) || parsed <= 0) {
    throw new Error("bridgePort must be a positive integer");
  }

  return parsed;
}

function normalizeHttpUrl(rawUrl: string, fieldName: string): string {
  let normalizedUrl: string;
  try {
    const url = new URL(rawUrl);
    if (!["http:", "https:"].includes(url.protocol)) {
      throw new Error("invalid scheme");
    }
    normalizedUrl = url.toString().replace(/\/$/, "");
  } catch {
    throw new Error(`${fieldName} must be a valid http(s) URL`);
  }

  return normalizedUrl;
}

function normalizeReplyMode(value: string | undefined): ReplyMode {
  const mode = value?.trim().toLowerCase() || "echo";
  if (mode === "echo" || mode === "matrix" || mode === "none") {
    return mode;
  }

  throw new Error("replyMode must be one of: echo, matrix, none");
}

function normalizeOptionalString(value: string | null | undefined): string | undefined {
  const trimmed = value?.trim();
  return trimmed ? trimmed : undefined;
}

function parseMatrixConfig(input: RawMatrixConfig | undefined): MatrixConfig | undefined {
  if (!input) {
    return undefined;
  }

  const homeserverUrl = normalizeOptionalString(input.homeserverUrl);
  const accessToken = normalizeOptionalString(input.accessToken);
  const roomId = normalizeOptionalString(input.roomId);
  const userId = normalizeOptionalString(input.userId);
  const botUserId = normalizeOptionalString(input.botUserId);

  if (!homeserverUrl && !accessToken && !roomId && !userId && !botUserId) {
    return undefined;
  }

  if (!homeserverUrl || !accessToken || !roomId) {
    throw new Error("matrix config requires homeserverUrl, accessToken, and roomId");
  }

  return {
    homeserverUrl: normalizeHttpUrl(homeserverUrl, "matrix.homeserverUrl"),
    accessToken,
    roomId,
    userId,
    botUserId
  };
}

function parseElevenLabsConfig(input: RawElevenLabsConfig | undefined): ElevenLabsConfig | undefined {
  if (!input) {
    return undefined;
  }

  const apiKey = normalizeOptionalString(input.apiKey);
  const voiceId = normalizeOptionalString(input.voiceId);
  const modelId = normalizeOptionalString(input.modelId) ?? "eleven_multilingual_v2";
  const outputFormat = normalizeOptionalString(input.outputFormat) ?? "mp3_44100_128";

  if (!apiKey && !voiceId) {
    return undefined;
  }

  if (!apiKey || !voiceId) {
    throw new Error("elevenLabs config requires apiKey and voiceId");
  }

  return {
    apiKey,
    voiceId,
    modelId,
    outputFormat
  };
}

function parseBasicAuthConfig(input: RawBasicAuthConfig | undefined): BasicAuthConfig | undefined {
  if (!input) {
    return undefined;
  }

  const username = normalizeOptionalString(input.username);
  const password = normalizeOptionalString(input.password);

  if (!username && !password) {
    return undefined;
  }

  if (!username || !password) {
    throw new Error("basicAuth config requires username and password");
  }

  return {
    username,
    password
  };
}

export function parseBridgeConfig(input: RawBridgeConfig): BridgeConfig {
  const bridgeHost = normalizeOptionalString(input.bridgeHost) ?? "0.0.0.0";
  const bridgePort = parsePort(input.bridgePort);
  const replyMode = normalizeReplyMode(input.replyMode);
  const bridgeBaseUrl = normalizeHttpUrl(
    input.bridgeBaseUrl ?? `http://localhost:${bridgePort}`,
    "bridgeBaseUrl"
  );
  const deviceAuthToken = normalizeOptionalString(input.deviceAuthToken);
  const basicAuth = parseBasicAuthConfig(input.basicAuth);
  const matrix = parseMatrixConfig(input.matrix);
  const elevenLabs = parseElevenLabsConfig(input.elevenLabs);

  if (replyMode === "matrix" && !matrix) {
    throw new Error("replyMode=matrix requires matrix config");
  }

  return {
    bridgeHost,
    bridgeBaseUrl,
    bridgePort,
    deviceAuthToken,
    basicAuth,
    replyMode,
    matrix,
    elevenLabs
  };
}

export function bridgeConfigFromEnv(env: NodeJS.ProcessEnv = process.env): BridgeConfig {
  return parseBridgeConfig({
    bridgeHost: env.BRIDGE_HOST,
    bridgeBaseUrl: env.BRIDGE_BASE_URL,
    bridgePort: env.PORT ?? env.BRIDGE_PORT,
    deviceAuthToken: env.DEVICE_AUTH_TOKEN,
    basicAuth: {
      username: env.BRIDGE_BASIC_AUTH_USERNAME,
      password: env.BRIDGE_BASIC_AUTH_PASSWORD
    },
    replyMode: env.BRIDGE_REPLY_MODE,
    matrix: {
      homeserverUrl: env.MATRIX_HOMESERVER_URL,
      accessToken: env.MATRIX_ACCESS_TOKEN,
      roomId: env.MATRIX_ROOM_ID,
      userId: env.MATRIX_USER_ID,
      botUserId: env.MATRIX_BOT_USER_ID
    },
    elevenLabs: {
      apiKey: env.ELEVENLABS_API_KEY,
      voiceId: env.ELEVENLABS_VOICE_ID,
      modelId: env.ELEVENLABS_MODEL_ID,
      outputFormat: env.ELEVENLABS_OUTPUT_FORMAT
    }
  });
}
