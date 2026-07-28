import assert from "node:assert/strict";
import { describe, it } from "node:test";

import { isAllowedCorsOrigin } from "./cors.js";

describe("isAllowedCorsOrigin", () => {
  const allowed = new Set(["http://127.0.0.1:5173", "http://localhost:5173"]);

  it("allows configured web app origins", () => {
    assert.equal(isAllowedCorsOrigin("http://127.0.0.1:5173", allowed), true);
    assert.equal(isAllowedCorsOrigin("http://localhost:5173", allowed), true);
  });

  it("allows Firefox and Chromium extension origins", () => {
    assert.equal(
      isAllowedCorsOrigin("moz-extension://abc123", allowed),
      true,
    );
    assert.equal(
      isAllowedCorsOrigin("chrome-extension://abc123", allowed),
      true,
    );
  });

  it("rejects unrelated origins", () => {
    assert.equal(isAllowedCorsOrigin("https://example.com", allowed), false);
    assert.equal(isAllowedCorsOrigin(undefined, allowed), false);
  });
});
