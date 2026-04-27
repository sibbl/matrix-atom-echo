import { describe, expect, it } from "vitest";

import { isDeviceAuthorized } from "../src/auth.js";
import { parseBridgeConfig } from "../src/config.js";

describe("bridge config", () => {
  it("validates and normalizes config", () => {
    const config = parseBridgeConfig({
      bridgeHost: "127.0.0.1",
      bridgeBaseUrl: "http://localhost:3000/",
      bridgePort: "3001",
      deviceAuthToken: " shared-secret ",
      basicAuth: {
        username: " admin ",
        password: " s3cret "
      },
      replyMode: "echo"
    });

    expect(config).toEqual({
      bridgeHost: "127.0.0.1",
      bridgeBaseUrl: "http://localhost:3000",
      bridgePort: 3001,
      deviceAuthToken: "shared-secret",
      basicAuth: {
        username: "admin",
        password: "s3cret"
      },
      replyMode: "echo",
      matrix: undefined,
      elevenLabs: undefined
    });
  });

  it("rejects an invalid base URL", () => {
    expect(() =>
      parseBridgeConfig({
        bridgeBaseUrl: "not a url"
      })
    ).toThrowError("bridgeBaseUrl must be a valid http(s) URL");
  });

  it("defaults to localhost echo mode when base URL is omitted", () => {
    const config = parseBridgeConfig({
      bridgePort: 4321
    });

    expect(config.bridgeBaseUrl).toBe("http://localhost:4321");
    expect(config.replyMode).toBe("echo");
  });

  it("requires matrix configuration when matrix reply mode is enabled", () => {
    expect(() =>
      parseBridgeConfig({
        replyMode: "matrix"
      })
    ).toThrowError("replyMode=matrix requires matrix config");
  });

  it("requires both basic auth username and password when basic auth is enabled", () => {
    expect(() =>
      parseBridgeConfig({
        basicAuth: {
          username: "admin"
        }
      })
    ).toThrowError("basicAuth config requires username and password");
  });
});

describe("device auth", () => {
  it("treats auth as optional when no shared secret is configured", () => {
    const config = parseBridgeConfig({
      bridgeBaseUrl: "http://localhost:3000",
      deviceAuthToken: "   "
    });

    expect(config.deviceAuthToken).toBeUndefined();
    expect(isDeviceAuthorized(undefined, config.deviceAuthToken)).toBe(true);
  });

  it("requires the configured shared secret when auth is enabled", () => {
    expect(isDeviceAuthorized("shared-secret", "shared-secret")).toBe(true);
    expect(isDeviceAuthorized("wrong-secret", "shared-secret")).toBe(false);
  });
});
