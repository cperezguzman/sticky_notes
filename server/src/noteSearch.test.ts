import assert from "node:assert/strict";
import { describe, it } from "node:test";

import { ilikeContainsPattern, noteMatchesQuery } from "./noteSearch.js";

describe("noteMatchesQuery", () => {
  it("matches title or body case-insensitively", () => {
    assert.equal(noteMatchesQuery("Grocery List", "milk eggs", "grocery"), true);
    assert.equal(noteMatchesQuery("List", "Buy MILK", "milk"), true);
    assert.equal(noteMatchesQuery("List", "bread", "milk"), false);
  });

  it("treats blank query as match-all", () => {
    assert.equal(noteMatchesQuery("A", "b", "  "), true);
    assert.equal(noteMatchesQuery("A", "b", ""), true);
  });
});

describe("ilikeContainsPattern", () => {
  it("wraps and escapes LIKE metacharacters", () => {
    assert.equal(ilikeContainsPattern("a%b_c\\d"), "%a\\%b\\_c\\\\d%");
    assert.equal(ilikeContainsPattern("  hello  "), "%hello%");
  });
});
