import assert from "node:assert/strict";
import { describe, it } from "node:test";

import {
  normalizeDomain,
  normalizeSourceUrl,
  rankContextHits,
  sourceDomainFromUrl,
  urlsMatchExact,
} from "./sourceUrl.js";

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
  it("extracts hostname and strips www", () => {
    assert.equal(sourceDomainFromUrl("https://WWW.Example.COM/a"), "example.com");
    assert.equal(sourceDomainFromUrl(""), "");
  });
});

describe("normalizeDomain / urlsMatchExact / rankContextHits", () => {
  it("normalizes www domains", () => {
    assert.equal(normalizeDomain("www.Example.com"), "example.com");
  });

  it("matches exact URLs ignoring hash", () => {
    assert.equal(
      urlsMatchExact("https://ex.com/a", "https://ex.com/a#frag"),
      true,
    );
    assert.equal(urlsMatchExact("https://ex.com/a", "https://ex.com/b"), false);
  });

  it("ranks exact hits before domain hits", () => {
    const hits = rankContextHits(
      [
        { id: 1, title: "Domain", sourceUrl: "https://ex.com/other" },
        { id: 2, title: "Exact", sourceUrl: "https://ex.com/page" },
        { id: 3, title: "None", sourceUrl: "" },
      ],
      "https://ex.com/page",
    );
    assert.equal(hits.length, 2);
    assert.equal(hits[0].id, 2);
    assert.equal(hits[0].match, "exact");
    assert.equal(hits[1].id, 1);
    assert.equal(hits[1].match, "domain");
  });
});
