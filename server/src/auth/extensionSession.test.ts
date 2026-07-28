import assert from "node:assert/strict";
import { describe, it } from "node:test";

import {
  getBearerSessionId,
  isExtensionClient,
} from "./extensionSession.js";

function fakeReq(headers: Record<string, string> = {}): {
  get(name: string): string | undefined;
  headers: Record<string, string>;
} {
  return {
    headers,
    get(name: string) {
      const key = Object.keys(headers).find(
        (h) => h.toLowerCase() === name.toLowerCase(),
      );
      return key ? headers[key] : undefined;
    },
  };
}

describe("extension session helpers", () => {
  it("detects extension client header", () => {
    assert.equal(
      isExtensionClient(fakeReq({ "X-Sticky-Client": "extension" }) as never),
      true,
    );
    assert.equal(isExtensionClient(fakeReq({}) as never), false);
  });

  it("parses bearer session id", () => {
    assert.equal(
      getBearerSessionId(
        fakeReq({ Authorization: "Bearer abc123" }) as never,
      ),
      "abc123",
    );
    assert.equal(getBearerSessionId(fakeReq({}) as never), null);
  });
});
