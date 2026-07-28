import assert from "node:assert/strict";
import { describe, it } from "node:test";

import { normalizeSourceUrl, sourceDomainFromUrl } from "./sourceUrl.js";

describe("normalizeSourceUrl", () => {
  it("accepts empty and http(s) URLs", () => {
    assert.deepEqual(normalizeSourceUrl(""), { ok: true, url: "" });
    assert.deepEqual(normalizeSourceUrl("  "), { ok: true, url: "" });
    const ok = normalizeSourceUrl("https://example.com/path");
    assert.equal(ok.ok, true);
    if (ok.ok) {
      assert.equal(ok.url, "https://example.com/path");
    }
  });

  it("rejects non-http schemes and garbage", () => {
    assert.equal(normalizeSourceUrl("javascript:alert(1)").ok, false);
    assert.equal(normalizeSourceUrl("ftp://x").ok, false);
    assert.equal(normalizeSourceUrl("not a url").ok, false);
    assert.equal(normalizeSourceUrl(42).ok, false);
  });
});

describe("sourceDomainFromUrl", () => {
  it("extracts hostname", () => {
    assert.equal(sourceDomainFromUrl("https://WWW.Example.COM/a"), "www.example.com");
    assert.equal(sourceDomainFromUrl(""), "");
  });
});
