export interface QueuedAudio {
  id: string;
  contentType: string;
  buffer: Buffer;
  createdAt: number;
  source?: "tts" | "matrix-audio" | "replay" | "echo";
}

export interface DeviceState {
  deviceId: string;
  queue: QueuedAudio[];
  lastAudio?: QueuedAudio;
  currentThreadEventId?: string;
  lastIncomingEventId?: string;
  lastRecordingAt?: number;
  lastDeliveryAt?: number;
  totalRecordings: number;
}

const deviceStates = new Map<string, DeviceState>();

function createDeviceState(deviceId: string): DeviceState {
  return {
    deviceId,
    queue: [],
    totalRecordings: 0
  };
}

export function resetAllDeviceState(): void {
  deviceStates.clear();
}

export function getDeviceState(deviceId: string): DeviceState {
  let state = deviceStates.get(deviceId);
  if (!state) {
    state = createDeviceState(deviceId);
    deviceStates.set(deviceId, state);
  }

  return state;
}

export function resetThread(deviceId: string): DeviceState {
  const state = getDeviceState(deviceId);
  state.currentThreadEventId = undefined;
  state.lastIncomingEventId = undefined;
  return state;
}

export function pushAudio(deviceId: string, audio: QueuedAudio): QueuedAudio {
  const state = getDeviceState(deviceId);
  state.queue.push(audio);
  state.lastAudio = audio;
  return audio;
}

export function popAudio(deviceId: string): QueuedAudio | undefined {
  const state = getDeviceState(deviceId);
  const audio = state.queue.shift();
  if (audio) {
    state.lastDeliveryAt = Date.now();
  }

  return audio;
}

export function replayLastAudio(deviceId: string): QueuedAudio | undefined {
  const state = getDeviceState(deviceId);
  if (!state.lastAudio) {
    return undefined;
  }

  const replay = {
    ...state.lastAudio,
    id: `${state.lastAudio.id}-replay-${state.queue.length + 1}`,
    createdAt: Date.now(),
    source: "replay" as const
  };

  state.queue.push(replay);
  return replay;
}

export function registerMatrixThreadRoot(deviceId: string, eventId: string): string {
  const state = getDeviceState(deviceId);
  if (!state.currentThreadEventId) {
    state.currentThreadEventId = eventId;
  }

  return state.currentThreadEventId;
}

export function markRecordingReceived(deviceId: string, recordedAt = Date.now()): DeviceState {
  const state = getDeviceState(deviceId);
  state.lastRecordingAt = recordedAt;
  state.totalRecordings += 1;
  return state;
}

export function markLastIncomingEvent(deviceId: string, eventId: string): DeviceState {
  const state = getDeviceState(deviceId);
  state.lastIncomingEventId = eventId;
  return state;
}

export function findDeviceIdByThreadRoot(threadRootEventId: string): string | undefined {
  for (const [deviceId, state] of deviceStates.entries()) {
    if (state.currentThreadEventId === threadRootEventId) {
      return deviceId;
    }
  }

  return undefined;
}

export function summarizeDeviceState(deviceId: string): Omit<DeviceState, "queue" | "lastAudio"> & {
  queueDepth: number;
  lastAudio?: Omit<QueuedAudio, "buffer"> & { byteLength: number };
} {
  const state = getDeviceState(deviceId);

  return {
    deviceId: state.deviceId,
    currentThreadEventId: state.currentThreadEventId,
    lastIncomingEventId: state.lastIncomingEventId,
    lastRecordingAt: state.lastRecordingAt,
    lastDeliveryAt: state.lastDeliveryAt,
    totalRecordings: state.totalRecordings,
    queueDepth: state.queue.length,
    lastAudio: state.lastAudio
      ? {
          id: state.lastAudio.id,
          contentType: state.lastAudio.contentType,
          createdAt: state.lastAudio.createdAt,
          source: state.lastAudio.source,
          byteLength: state.lastAudio.buffer.length
        }
      : undefined
  };
}
