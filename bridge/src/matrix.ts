import { randomUUID } from "node:crypto";

import type { MatrixConfig } from "./config.js";
import type { AudioDownloadClient, AudioPayload } from "./replies.js";

export interface SentRecordingResult {
  eventId: string;
}

export interface MatrixClient {
  sendDeviceRecording(args: {
    deviceId: string;
    contentType: string;
    buffer: Buffer;
    threadRootEventId?: string;
  }): Promise<SentRecordingResult>;
}

function joinPath(baseUrl: string, path: string): string {
  return new URL(path, `${baseUrl}/`).toString();
}

function buildThreadRelation(threadRootEventId: string): Record<string, unknown> {
  return {
    "m.relates_to": {
      rel_type: "m.thread",
      event_id: threadRootEventId,
      is_falling_back: true,
      "m.in_reply_to": {
        event_id: threadRootEventId
      }
    }
  };
}

function parseMxcUri(mxcUri: string): { serverName: string; mediaId: string } {
  const match = mxcUri.match(/^mxc:\/\/([^/]+)\/(.+)$/);
  if (!match) {
    throw new Error("invalid Matrix content URI");
  }

  return {
    serverName: match[1],
    mediaId: match[2]
  };
}

export class MatrixApiClient implements MatrixClient, AudioDownloadClient {
  constructor(private readonly config: MatrixConfig) {}

  private async jsonRequest<T>(url: string, init: RequestInit): Promise<T> {
    const response = await fetch(url, {
      ...init,
      headers: {
        Authorization: `Bearer ${this.config.accessToken}`,
        ...(init.headers ?? {})
      }
    });

    if (!response.ok) {
      throw new Error(`Matrix API request failed with status ${response.status}`);
    }

    return (await response.json()) as T;
  }

  async sendDeviceRecording(args: {
    deviceId: string;
    contentType: string;
    buffer: Buffer;
    threadRootEventId?: string;
  }): Promise<SentRecordingResult> {
    const filename = `${args.deviceId}-${Date.now()}.wav`;
    const uploadUrl = new URL(joinPath(this.config.homeserverUrl, "/_matrix/media/v3/upload"));
    uploadUrl.searchParams.set("filename", filename);

    const uploadResponse = await this.jsonRequest<{ content_uri: string }>(uploadUrl.toString(), {
      method: "POST",
      headers: {
        "Content-Type": args.contentType
      },
      body: args.buffer
    });

    const txnId = randomUUID();
    const sendUrl = joinPath(
      this.config.homeserverUrl,
      `/_matrix/client/v3/rooms/${encodeURIComponent(this.config.roomId)}/send/m.room.message/${txnId}`
    );

    const body: Record<string, unknown> = {
      msgtype: "m.audio",
      body: filename,
      filename,
      url: uploadResponse.content_uri,
      info: {
        mimetype: args.contentType,
        size: args.buffer.length
      },
      ...(args.threadRootEventId ? buildThreadRelation(args.threadRootEventId) : {})
    };

    return this.jsonRequest<SentRecordingResult>(sendUrl, {
      method: "PUT",
      headers: {
        "Content-Type": "application/json"
      },
      body: JSON.stringify(body)
    });
  }

  async downloadAudio(sourceUrl: string): Promise<AudioPayload> {
    const { mediaId, serverName } = parseMxcUri(sourceUrl);
    const url = joinPath(
      this.config.homeserverUrl,
      `/_matrix/media/v3/download/${encodeURIComponent(serverName)}/${encodeURIComponent(mediaId)}`
    );

    const response = await fetch(url, {
      headers: {
        Authorization: `Bearer ${this.config.accessToken}`
      }
    });

    if (!response.ok) {
      throw new Error(`Matrix media download failed with status ${response.status}`);
    }

    const arrayBuffer = await response.arrayBuffer();

    return {
      id: `matrix-download-${randomUUID()}`,
      contentType: response.headers.get("content-type") ?? "application/octet-stream",
      buffer: Buffer.from(arrayBuffer),
      createdAt: Date.now()
    };
  }
}
