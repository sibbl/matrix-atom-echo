import { Buffer } from "node:buffer";

import { describe, expect, it } from "vitest";

import {
  isBasicAuthAuthorized,
  readBasicAuthCredentials
} from "../src/auth.js";

describe("basic auth", () => {
  it("parses basic auth credentials from the Authorization header", () => {
    const encoded = Buffer.from("admin:s3cret", "utf8").toString("base64");

    expect(
      readBasicAuthCredentials({
        authorization: `Basic ${encoded}`
      })
    ).toEqual({
      username: "admin",
      password: "s3cret"
    });
  });

  it("authorizes matching configured credentials", () => {
    expect(
      isBasicAuthAuthorized(
        {
          username: "admin",
          password: "s3cret"
        },
        {
          username: "admin",
          password: "s3cret"
        }
      )
    ).toBe(true);
    expect(
      isBasicAuthAuthorized(
        {
          username: "admin",
          password: "wrong"
        },
        {
          username: "admin",
          password: "s3cret"
        }
      )
    ).toBe(false);
  });
});
