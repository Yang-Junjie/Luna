#!/usr/bin/env node

import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";

const [clientPath, legacyHostPath, authoringHostPath] = process.argv.slice(2);
assert.ok(clientPath, "Expected the TS client path.");
assert.ok(legacyHostPath, "Expected the LunaCLI host path.");
assert.ok(authoringHostPath, "Expected the LunaAuthoringHost path.");

const input = [
    "new",
    "primitive InteractiveCube Cube",
    "transform InteractiveCube 0 0 0 0 0 0 1 1 1",
    "inspect entity InteractiveCube",
    "verify component InteractiveCube Mesh",
    "run",
    "exit",
    "",
].join("\n");

const result = spawnSync(
    process.execPath,
    [
        "--disable-warning=ExperimentalWarning",
        "--experimental-strip-types",
        clientPath,
        "--host",
        legacyHostPath,
        "--authoring-host",
        authoringHostPath,
        "--json",
        "interactive",
    ],
    {
        encoding: "utf8",
        input,
        stdio: ["pipe", "pipe", "pipe"],
    },
);

if (result.status !== 0) {
    console.error(result.stdout);
    console.error(result.stderr);
}

assert.equal(result.status, 0);
assert.match(result.stdout, /"ok": true/);
assert.match(result.stdout, /"alias": "InteractiveCube"/);
