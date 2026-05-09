#!/usr/bin/env node

import assert from "node:assert/strict";

import {
    AUTHORING_PROTOCOL_NAME,
    AUTHORING_PROTOCOL_VERSION,
    AuthoringPlanBuilder,
    buildAuthoringRepairContext,
    findEntitiesByComponent,
    findEntityBinding,
    findEntityByName,
    findEntityByRef,
    findEntityByUuid,
    latestHierarchy,
    latestHierarchyEntities,
    normalizeAuthoringReport,
    normalizeAuthoringCapabilities,
    normalizeAuthoringPlan,
    readAuthoringPlanJson,
    writeAuthoringPlanJson,
} from "../client/authoringProtocol.ts";

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
    .snapshot()
    .summary()
    .toPlan();

assert.equal(builtPlan.protocol?.name, AUTHORING_PROTOCOL_NAME);
assert.equal(builtPlan.protocol?.version, AUTHORING_PROTOCOL_VERSION);
assert.equal(builtPlan.commands.length, 26);

const serializedPlan = writeAuthoringPlanJson(builtPlan);
const normalizedPlan = normalizeAuthoringPlan(JSON.parse(serializedPlan) as unknown);
assert.equal(normalizedPlan.commands.length, builtPlan.commands.length);
assert.deepEqual(normalizedPlan.commands[20], { op: "verify", check: "sceneSaved" });
assert.deepEqual(normalizedPlan.commands[24], { op: "snapshot" });

const validFixture = readAuthoringPlanJson(validFixturePath);
assert.equal(validFixture.protocol?.name, AUTHORING_PROTOCOL_NAME);
assert.equal(validFixture.protocol?.version, AUTHORING_PROTOCOL_VERSION);
assert.ok(validFixture.commands.length > 0);

assert.throws(
    () => readAuthoringPlanJson(invalidFixturePath),
    /Unsupported plan op 'spawn'\./,
);
assert.throws(
    () => normalizeAuthoringPlan({
        commands: [
            { op: "primitive", alias: "Box", mesh: "Cube", material: "Default" },
        ],
    }),
    /Plan field 'commands\[0\]\.material' is not supported\./,
);
assert.throws(
    () => normalizeAuthoringPlan({
        protocol: { name: AUTHORING_PROTOCOL_NAME, version: AUTHORING_PROTOCOL_VERSION, extra: true },
        commands: [],
    }),
    /Plan field 'protocol\.extra' is not supported\./,
);

const capabilities = normalizeAuthoringCapabilities({
    protocol: { name: AUTHORING_PROTOCOL_NAME, version: AUTHORING_PROTOCOL_VERSION },
    capabilities: [
        {
            op: "primitive",
            title: "Create Primitive",
            description: "Create an entity using a builtin primitive mesh.",
            effects: ["mutatesScene"],
            requiresConfirmation: false,
            parameters: {
                mesh: {
                    type: "string",
                    required: true,
                    description: "Builtin primitive mesh name.",
                    enum: ["Cube", "Sphere", "Plane", "Cylinder", "Cone"],
                },
            },
            examples: [
                {
                    description: "Create a cube.",
                    command: { op: "primitive", alias: "Cube", mesh: "Cube" },
                },
            ],
        },
    ],
});

assert.equal(capabilities.capabilities.length, 1);
assert.equal(capabilities.capabilities[0]?.parameters.mesh?.enum?.[0], "Cube");
for (const capability of capabilities.capabilities) {
    for (const example of capability.examples) {
        if (typeof example.command === "string") {
            continue;
        }
        assert.equal(example.command.op, capability.op);
        normalizeAuthoringPlan({ commands: [example.command] });
    }
}

const report = normalizeAuthoringReport({
    protocol: { name: AUTHORING_PROTOCOL_NAME, version: AUTHORING_PROTOCOL_VERSION },
    ok: false,
    scene: { name: "Untitled", path: null, entityCount: 0, dirty: false },
    entities: [
        {
            alias: "Box",
            uuid: "42",
            name: "Cube",
        },
    ],
    savedScenes: [],
    inspections: [
        {
            type: "hierarchy",
            ref: "hierarchy",
            entities: [
                {
                    ref: "Box",
                    uuid: "42",
                    name: "Cube",
                    parentUuid: null,
                    children: [],
                    components: ["ID", "Transform", "Mesh"],
                    transform: {
                        translation: [1, 2, 3],
                        rotationDeg: [0, 45, 0],
                        scale: [1, 1, 1],
                    },
                    mesh: {
                        meshHandle: "1001",
                        firstSubmesh: 1,
                        submeshCount: 2,
                        submeshMaterials: ["2001", null],
                    },
                },
            ],
        },
    ],
    verifications: [
        {
            type: "hasComponent",
            ok: true,
            ref: "Box",
            uuid: "42",
            component: "Mesh",
            expectedCount: 0,
            actualCount: 0,
            message: "Entity has component.",
        },
    ],
    diagnostics: [
        {
            severity: "error",
            phase: "validate",
            code: "UnknownEntity",
            message: "Unknown entity reference 'Missing'.",
            commandIndex: 1,
            command: "transform",
            field: "commands[1].entity",
            entityRef: "Missing",
            component: null,
            path: null,
            recoverable: true,
            expected: "resolvable entity reference",
            actual: "Missing",
            suggestedCommand: null,
        },
    ],
    errors: ["Unknown entity reference 'Missing'."],
});

assert.equal(report.entities[0]?.uuid, "42");
assert.equal(report.inspections[0]?.type, "hierarchy");
assert.equal(report.inspections[0]?.entities[0]?.transform?.translation[1], 2);
assert.equal(report.inspections[0]?.entities[0]?.mesh?.firstSubmesh, 1);
assert.equal(report.verifications[0]?.component, "Mesh");
assert.equal(latestHierarchy(report)?.ref, "hierarchy");
assert.equal(latestHierarchyEntities(report).length, 1);
assert.equal(findEntityBinding(report, "Box")?.uuid, "42");
assert.equal(findEntityByUuid(report, "42")?.name, "Cube");
assert.equal(findEntityByRef(report, "Box")?.uuid, "42");
assert.equal(findEntityByRef(report, "42")?.name, "Cube");
assert.equal(findEntityByName(report, "Cube")?.uuid, "42");
assert.equal(findEntitiesByComponent(report, "Mesh").length, 1);

const repair = buildAuthoringRepairContext(report);
assert.equal(repair.ok, false);
assert.equal(repair.retryable, true);
assert.equal(repair.diagnostics[0]?.code, "UnknownEntity");
