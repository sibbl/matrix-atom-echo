import { randomUUID } from "node:crypto";

import { assertAudioPayload } from "./audio.js";
import type { ReplyMode } from "./config.js";
import {
  findDeviceIdByThreadRoot,
  getDeviceState,
  markLastIncomingEvent,
  markRecordingReceived,
  popAudio,
  pushAudio,
  registerMatrixThreadRoot,
  replayLastAudio,
  resetThread,
  summarizeDeviceState,
  type QueuedAudio
} from "./devices.js";
import type { MatrixClient } from "./matrix.js";
import {
  queueBotAudioReply,
  queueBotTextReply,
  registerOutgoingMatrixMessage,
  type AudioDownloadClient,
  type TextToSpeechClient
} from "./replies.js";

export interface SubmitRecordingResult {
  accepted: true;
  eventId: string;
  threadRootEventId: string;
  isNewThread: boolean;
  queuedAudioId?: string;
}

export interface BridgeDependencies {
  replyMode: ReplyMode;
  ttsClient: TextToSpeechClient;
  audioDownloadClient: AudioDownloadClient;
  matrixClient?: MatrixClient;
  now?: () => number;
}

export class BridgeService {
  private readonly now: () => number;

  constructor(private readonly dependencies: BridgeDependencies) {
    this.now = dependencies.now ?? Date.now;
  }

  submitRecording(deviceId: string, contentType: string | undefined, buffer: Buffer): Promise<SubmitRecordingResult> {
    assertAudioPayload(contentType, buffer);
    markRecordingReceived(deviceId, this.now());

    if (this.dependencies.replyMode === "echo") {
      return Promise.resolve(this.handleEchoRecording(deviceId, contentType!, buffer));
    }

    if (this.dependencies.replyMode === "none") {
      return Promise.resolve(this.handleNoReplyRecording(deviceId));
    }

    return this.handleMatrixRecording(deviceId, contentType!, buffer);
  }

  popNextAudio(deviceId: string): QueuedAudio | undefined {
    return popAudio(deviceId);
  }

  replayLastAudio(deviceId: string): QueuedAudio | undefined {
    return replayLastAudio(deviceId);
  }

  resetDeviceThread(deviceId: string): ReturnType<typeof summarizeDeviceState> {
    resetThread(deviceId);
    return summarizeDeviceState(deviceId);
  }

  getDeviceSummary(deviceId: string): ReturnType<typeof summarizeDeviceState> {
    return summarizeDeviceState(deviceId);
  }

  async queueTextReplyForDevice(deviceId: string, text: string): Promise<QueuedAudio> {
    return queueBotTextReply(deviceId, text, this.dependencies.ttsClient);
  }

  async queueAudioReplyForDevice(deviceId: string, sourceUrl: string): Promise<QueuedAudio> {
    return queueBotAudioReply(deviceId, sourceUrl, this.dependencies.audioDownloadClient);
  }

  async queueTextReplyForThread(threadRootEventId: string, text: string): Promise<QueuedAudio> {
    const deviceId = this.resolveDeviceId(threadRootEventId);
    return this.queueTextReplyForDevice(deviceId, text);
  }

  async queueAudioReplyForThread(threadRootEventId: string, sourceUrl: string): Promise<QueuedAudio> {
    const deviceId = this.resolveDeviceId(threadRootEventId);
    return this.queueAudioReplyForDevice(deviceId, sourceUrl);
  }

  private resolveDeviceId(threadRootEventId: string): string {
    const deviceId = findDeviceIdByThreadRoot(threadRootEventId);
    if (!deviceId) {
      throw new Error("thread root is not associated with a device");
    }

    return deviceId;
  }

  private handleEchoRecording(
    deviceId: string,
    contentType: string,
    buffer: Buffer
  ): SubmitRecordingResult {
    const eventId = `local-${randomUUID()}`;
    const { isNewThread, threadRootEventId } = registerOutgoingMatrixMessage(deviceId, eventId);
    markLastIncomingEvent(deviceId, eventId);

    const queuedAudio = pushAudio(deviceId, {
      id: `echo-${randomUUID()}`,
      contentType,
      buffer: Buffer.from(buffer),
      createdAt: this.now(),
      source: "echo"
    });

    return {
      accepted: true,
      eventId,
      threadRootEventId,
      isNewThread,
      queuedAudioId: queuedAudio.id
    };
  }

  private handleNoReplyRecording(deviceId: string): SubmitRecordingResult {
    const eventId = `local-${randomUUID()}`;
    const { isNewThread, threadRootEventId } = registerOutgoingMatrixMessage(deviceId, eventId);
    markLastIncomingEvent(deviceId, eventId);

    return {
      accepted: true,
      eventId,
      threadRootEventId,
      isNewThread
    };
  }

  private async handleMatrixRecording(
    deviceId: string,
    contentType: string,
    buffer: Buffer
  ): Promise<SubmitRecordingResult> {
    const matrixClient = this.dependencies.matrixClient;
    if (!matrixClient) {
      throw new Error("matrix reply mode requires a Matrix client");
    }

    const threadRootEventId = getDeviceState(deviceId).currentThreadEventId;
    const result = await matrixClient.sendDeviceRecording({
      deviceId,
      contentType,
      buffer,
      threadRootEventId
    });

    const registration = registerOutgoingMatrixMessage(deviceId, result.eventId);
    markLastIncomingEvent(deviceId, result.eventId);

    return {
      accepted: true,
      eventId: result.eventId,
      threadRootEventId: registration.threadRootEventId,
      isNewThread: registration.isNewThread
    };
  }
}

export function seedThreadRoot(deviceId: string, threadRootEventId: string): string {
  return registerMatrixThreadRoot(deviceId, threadRootEventId);
}
