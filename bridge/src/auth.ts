import { timingSafeEqual } from "node:crypto";
import type { IncomingHttpHeaders } from "node:http";

export interface BasicAuthConfig {
  username: string;
  password: string;
}

export interface BasicAuthCredentials {
  username: string;
  password: string;
}

function safeEqual(left: string, right: string): boolean {
  const leftBuffer = Buffer.from(left, "utf8");
  const rightBuffer = Buffer.from(right, "utf8");

  if (leftBuffer.length !== rightBuffer.length) {
    return false;
  }

  return timingSafeEqual(leftBuffer, rightBuffer);
}

export function isDeviceAuthorized(providedToken: string | undefined, expectedToken?: string): boolean {
  if (!expectedToken) {
    return true;
  }

  return providedToken === expectedToken;
}

export function isBasicAuthAuthorized(
  providedCredentials: BasicAuthCredentials | undefined,
  expectedCredentials?: BasicAuthConfig
): boolean {
  if (!expectedCredentials) {
    return true;
  }

  if (!providedCredentials) {
    return false;
  }

  return (
    safeEqual(providedCredentials.username, expectedCredentials.username) &&
    safeEqual(providedCredentials.password, expectedCredentials.password)
  );
}

export function readDeviceToken(headers: IncomingHttpHeaders): string | undefined {
  const authHeader = headers.authorization;
  if (typeof authHeader === "string") {
    const match = authHeader.match(/^Bearer\s+(.+)$/i);
    if (match?.[1]) {
      return match[1].trim();
    }
  }

  const deviceToken = headers["x-device-token"];
  if (typeof deviceToken === "string" && deviceToken.trim()) {
    return deviceToken.trim();
  }

  return undefined;
}

export function readBasicAuthCredentials(headers: IncomingHttpHeaders): BasicAuthCredentials | undefined {
  const authHeader = headers.authorization;
  if (typeof authHeader !== "string") {
    return undefined;
  }

  const match = authHeader.match(/^Basic\s+(.+)$/i);
  if (!match?.[1]) {
    return undefined;
  }

  let decoded: string;
  try {
    decoded = Buffer.from(match[1], "base64").toString("utf8");
  } catch {
    return undefined;
  }

  const separatorIndex = decoded.indexOf(":");
  if (separatorIndex === -1) {
    return undefined;
  }

  return {
    username: decoded.slice(0, separatorIndex),
    password: decoded.slice(separatorIndex + 1)
  };
}
