import { beforeEach, describe, expect, it } from "vitest";

import {
  findDeviceIdByThreadRoot,
  getDeviceState,
  markRecordingReceived,
  popAudio,
  pushAudio,
  replayLastAudio,
  resetAllDeviceState,
  resetThread,
  summarizeDeviceState
} from "../src/devices.js";

describe("device state", () => {
  beforeEach(() => {
    resetAllDeviceState();
  });

  it("creates device state lazily", () => {
    const state = getDeviceState("default");

    expect(state.deviceId).toBe("default");
    expect(state.queue).toEqual([]);
    expect(state.totalRecordings).toBe(0);
    expect(state.currentThreadEventId).toBeUndefined();
  });

  it("resets thread state", () => {
    const state = getDeviceState("reset-test");
    state.currentThreadEventId = "$root";
    state.lastIncomingEventId = "$reply";

    resetThread("reset-test");

    expect(state.currentThreadEventId).toBeUndefined();
    expect(state.lastIncomingEventId).toBeUndefined();
  });

  it("pushes and pops audio", () => {
    pushAudio("audio-test", {
      id: "a1",
      contentType: "audio/wav",
      buffer: Buffer.from("abc"),
      createdAt: 1
    });

    const state = getDeviceState("audio-test");

    expect(state.lastAudio?.id).toBe("a1");
    expect(state.queue).toHaveLength(1);

    const popped = popAudio("audio-test");

    expect(popped?.id).toBe("a1");
    expect(state.queue).toHaveLength(0);
    expect(state.lastAudio?.id).toBe("a1");
  });

  it("replays the last audio item", () => {
    pushAudio("replay-test", {
      id: "last-audio",
      contentType: "audio/wav",
      buffer: Buffer.from("abc"),
      createdAt: 1
    });
    popAudio("replay-test");

    const replayed = replayLastAudio("replay-test");
    const state = getDeviceState("replay-test");

    expect(replayed?.id).toContain("last-audio-replay");
    expect(state.queue).toHaveLength(1);
    expect(state.queue[0]?.source).toBe("replay");
  });

  it("can resolve devices by thread root and summarize state", () => {
    const state = getDeviceState("summary-test");
    state.currentThreadEventId = "$root";
    markRecordingReceived("summary-test", 123);

    pushAudio("summary-test", {
      id: "audio-summary",
      contentType: "audio/wav",
      buffer: Buffer.from("abc"),
      createdAt: 200
    });

    expect(findDeviceIdByThreadRoot("$root")).toBe("summary-test");
    expect(summarizeDeviceState("summary-test")).toEqual({
      deviceId: "summary-test",
      currentThreadEventId: "$root",
      lastIncomingEventId: undefined,
      lastRecordingAt: 123,
      lastDeliveryAt: undefined,
      totalRecordings: 1,
      queueDepth: 1,
      lastAudio: {
        id: "audio-summary",
        contentType: "audio/wav",
        createdAt: 200,
        source: undefined,
        byteLength: 3
      }
    });
  });
});
