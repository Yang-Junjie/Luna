#!/usr/bin/env node

import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import {
    findEntitiesByComponent,
    findEntityByName,
    latestHierarchy,
    normalizeAuthoringCapabilities,
    normalizeAuthoringPlan,
    normalizeAuthoringReport,
} from "../client/authoringProtocol.ts";

const [clientPath, legacyHostPath, authoringHostPath] = process.argv.slice(2);
assert.ok(clientPath, "Expected the TS client path.");
assert.ok(legacyHostPath, "Expected the LunaCLI host path.");
assert.ok(authoringHostPath, "Expected the LunaAuthoringHost path.");

const capabilities = spawnSync(
    process.execPath,
    [
        "--disable-warning=ExperimentalWarning",
        "--experimental-strip-types",
        clientPath,
        "--authoring-host",
        authoringHostPath,
        "capabilities",
    ],
    { encoding: "utf8" },
);

assert.equal(capabilities.status, 0, capabilities.stderr);
assert.match(capabilities.stdout, /"capabilities"/);
assert.match(capabilities.stdout, /"op": "primitive"/);

const capabilitiesDocument = normalizeAuthoringCapabilities(JSON.parse(capabilities.stdout) as unknown);
assert.equal(capabilitiesDocument.capabilities.length, 21);
assert.equal(new Set(capabilitiesDocument.capabilities.map((capability) => capability.op)).size, 21);
for (const capability of capabilitiesDocument.capabilities) {
    assert.ok(capability.examples.length > 0, `Expected examples for ${capability.op}.`);
    for (const example of capability.examples) {
        if (typeof example.command === "string") {
            continue;
        }
        assert.equal(example.command.op, capability.op);
        normalizeAuthoringPlan({ commands: [example.command] });
    }
}

const composed = spawnSync(
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
        "compose",
        "create a simple scene with a cube, floor, camera, and light",
    ],
    { encoding: "utf8" },
);

assert.equal(composed.status, 0, composed.stderr);
const composedResult = JSON.parse(composed.stdout) as {
    ok: boolean;
    mode: string;
    plan: { commands: Array<Record<string, unknown>> };
    report: unknown;
    repair: { ok: boolean; diagnostics: unknown[]; retryable: boolean };
};
assert.equal(composedResult.ok, true);
assert.equal(composedResult.mode, "dry-run");
const composedReport = normalizeAuthoringReport(composedResult.report);
assert.equal(composedReport.ok, true);
assert.deepEqual(composedResult.repair.diagnostics, []);
assert.equal(composedResult.repair.retryable, false);
assert.ok(composedResult.plan.commands.some((command) => command.op === "primitive" && command.alias === "Cube"));
assert.ok(composedResult.plan.commands.some((command) => command.op === "primitive" && command.alias === "Floor"));
assert.ok(composedResult.plan.commands.some((command) => command.op === "snapshot"));

const executed = spawnSync(
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
        "compose",
        "create a simple scene with a cube",
        "--execute",
    ],
    { encoding: "utf8" },
);

assert.equal(executed.status, 0, executed.stderr);
const executedResult = JSON.parse(executed.stdout) as { report: unknown };
const executedReport = normalizeAuthoringReport(executedResult.report);
assert.ok(latestHierarchy(executedReport));
assert.equal(findEntityByName(executedReport, "Cube")?.components.includes("Mesh"), true);
assert.ok(findEntitiesByComponent(executedReport, "Transform").length > 0);

const planOnly = spawnSync(
    process.execPath,
    [
        "--disable-warning=ExperimentalWarning",
        "--experimental-strip-types",
        clientPath,
        "--host",
        legacyHostPath,
        "--authoring-host",
        authoringHostPath,
        "compose",
        "create a sphere scene",
        "--plan-only",
    ],
    { encoding: "utf8" },
);

assert.equal(planOnly.status, 0, planOnly.stderr);
assert.match(planOnly.stdout, /"alias": "Sphere"/);
assert.doesNotMatch(planOnly.stdout, /"repair"/);
