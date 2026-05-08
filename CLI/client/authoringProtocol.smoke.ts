#!/usr/bin/env node

import assert from "node:assert/strict";

import {
    AUTHORING_PROTOCOL_NAME,
    AUTHORING_PROTOCOL_VERSION,
    AuthoringPlanBuilder,
    normalizeAuthoringPlan,
    readAuthoringPlanJson,
    writeAuthoringPlanJson,
} from "./authoringProtocol.ts";

const [validFixturePath, invalidFixturePath] = process.argv.slice(2);
assert.ok(validFixturePath, "Expected a valid authoring fixture path.");
assert.ok(invalidFixturePath, "Expected an invalid authoring fixture path.");

const builtPlan = new AuthoringPlanBuilder()
    .newScene()
    .openScene("Scenes/Existing.lunascene")
    .saveScene("Scenes/Saved.lunascene")
    .entity("Root", "Root Entity")
    .camera("MainCamera")
    .directionalLight("Sun")
    .pointLight("Lamp")
    .spotLight("Spot")
    .primitive("Box", "Cube")
    .parent("Box", "Root")
    .unparent("Box")
    .rename("Box", "Renamed Box")
    .transform("Box", [1, 2, 3], [10, 20, 30], [1.5, 2, 0.5])
    .lightIntensity("Sun", 4.5)
    .lightColor("Sun", [0.5, 0.25, 0.75])
    .cameraPerspective("MainCamera", 60, 0.125, 1024)
    .cameraOrthographic("MainCamera", 12, 0.25, 512)
    .inspectScene()
    .inspectEntity("Box")
    .inspectHierarchy()
    .verifySceneSaved()
    .verifyEntityExists("Box")
    .verifyHasComponent("Box", "Mesh")
    .verifyEntityCountAtLeast(5)
    .summary()
    .toPlan();

assert.equal(builtPlan.protocol?.name, AUTHORING_PROTOCOL_NAME);
assert.equal(builtPlan.protocol?.version, AUTHORING_PROTOCOL_VERSION);
assert.equal(builtPlan.commands.length, 25);

const serializedPlan = writeAuthoringPlanJson(builtPlan);
const normalizedPlan = normalizeAuthoringPlan(JSON.parse(serializedPlan) as unknown);
assert.equal(normalizedPlan.commands.length, builtPlan.commands.length);
assert.deepEqual(normalizedPlan.commands[20], { op: "verify", check: "sceneSaved" });

const validFixture = readAuthoringPlanJson(validFixturePath);
assert.equal(validFixture.protocol?.name, AUTHORING_PROTOCOL_NAME);
assert.equal(validFixture.protocol?.version, AUTHORING_PROTOCOL_VERSION);
assert.ok(validFixture.commands.length > 0);

assert.throws(
    () => readAuthoringPlanJson(invalidFixturePath),
    /Unsupported plan op 'spawn'\./,
);
