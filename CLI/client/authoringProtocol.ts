import { readFileSync } from "node:fs";

export const AUTHORING_PROTOCOL_NAME = "luna.authoring" as const;
export const AUTHORING_PROTOCOL_VERSION = 1 as const;

export type Vec3 = [number, number, number];

export type AuthoringProtocolInfo = {
    name: typeof AUTHORING_PROTOCOL_NAME;
    version: typeof AUTHORING_PROTOCOL_VERSION;
};

export type PlanCommand =
    | { op: "new" }
    | { op: "open"; path: string }
    | { op: "save"; path: string }
    | { op: "entity"; alias: string; name: string }
    | { op: "camera"; alias: string }
    | { op: "directional-light"; alias: string }
    | { op: "point-light"; alias: string }
    | { op: "spot-light"; alias: string }
    | { op: "primitive"; alias: string; mesh: string }
    | { op: "parent"; child: string; parent: string }
    | { op: "unparent"; child: string }
    | { op: "name"; entity: string; name: string }
    | { op: "transform"; entity: string; translation: Vec3; rotationDeg?: Vec3; scale?: Vec3 }
    | { op: "light-intensity"; entity: string; value: number }
    | { op: "light-color"; entity: string; color: Vec3 }
    | { op: "camera-perspective"; entity: string; fovDeg: number; near: number; far: number }
    | { op: "camera-orthographic"; entity: string; size: number; near: number; far: number }
    | { op: "inspect"; target: "scene" }
    | { op: "inspect"; target: "hierarchy" }
    | { op: "inspect"; target: "entity"; entity: string }
    | { op: "verify"; check: "sceneSaved" }
    | { op: "verify"; check: "entityExists"; entity: string }
    | { op: "verify"; check: "hasComponent"; entity: string; component: string }
    | { op: "verify"; check: "entityCountAtLeast"; count: number }
    | { op: "summary" }
    | { op: "snapshot" };

export type AuthoringPlan = {
    protocol?: AuthoringProtocolInfo;
    project?: string;
    commands: PlanCommand[];
};

export type AuthoringCapabilityParameterType = "string" | "number" | "integer" | "boolean" | "vec3";

export type AuthoringCapabilityParameter = {
    type: AuthoringCapabilityParameterType;
    required: boolean;
    description: string;
    enum?: string[];
};

export type AuthoringCapabilityExample = {
    description: string;
    command: PlanCommand | string;
};

export type AuthoringCapability = {
    op: string;
    title: string;
    description: string;
    effects: string[];
    requiresConfirmation: boolean;
    parameters: Record<string, AuthoringCapabilityParameter>;
    examples: AuthoringCapabilityExample[];
};

export type AuthoringCapabilitiesDocument = {
    protocol: AuthoringProtocolInfo;
    capabilities: AuthoringCapability[];
};

export type AuthoringDiagnostic = {
    severity: "info" | "warning" | "error";
    phase: "parse" | "validate" | "execute" | "verify";
    code: string;
    message: string;
    commandIndex: number | null;
    command: string | null;
    field: string | null;
    entityRef: string | null;
    component: string | null;
    path: string | null;
    recoverable: boolean;
    expected: string | null;
    actual: string | null;
    suggestedCommand: string | null;
};

export type AuthoringSceneSnapshot = {
    name: string;
    path: string | null;
    entityCount: number;
    dirty: boolean;
};

export type AuthoringEntityBinding = {
    alias: string;
    uuid: string;
    name: string;
};

export type AuthoringTransformInspection = {
    translation: Vec3;
    rotationDeg: Vec3;
    scale: Vec3;
};

export type AuthoringCameraInspection = {
    primary: boolean;
    fixedAspectRatio: boolean;
    projection: string;
    perspectiveFovDeg: number;
    perspectiveNear: number;
    perspectiveFar: number;
    orthographicSize: number;
    orthographicNear: number;
    orthographicFar: number;
};

export type AuthoringLightInspection = {
    type: string;
    enabled: boolean;
    color: Vec3;
    intensity: number;
    range: number;
    innerConeAngleDeg: number;
    outerConeAngleDeg: number;
};

export type AuthoringMeshInspection = {
    meshHandle: string | null;
    firstSubmesh?: number;
    submeshCount?: number;
    submeshMaterials: Array<string | null>;
};

export type AuthoringEntityInspection = {
    ref: string;
    uuid: string | null;
    name: string;
    parentUuid: string | null;
    children: Array<string | null>;
    components: string[];
    transform?: AuthoringTransformInspection;
    camera?: AuthoringCameraInspection;
    light?: AuthoringLightInspection;
    mesh?: AuthoringMeshInspection;
};

export type AuthoringInspection = {
    type: "scene" | "entity" | "hierarchy";
    ref: string;
    entities: AuthoringEntityInspection[];
};

export type AuthoringVerification = {
    type: "sceneSaved" | "entityExists" | "hasComponent" | "entityCountAtLeast";
    ok: boolean;
    ref: string;
    uuid: string | null;
    component: string;
    expectedCount: number;
    actualCount: number;
    message: string;
};

export type AuthoringReport = {
    protocol: AuthoringProtocolInfo;
    ok: boolean;
    scene: AuthoringSceneSnapshot;
    entities: AuthoringEntityBinding[];
    savedScenes: string[];
    inspections: AuthoringInspection[];
    verifications: AuthoringVerification[];
    diagnostics: AuthoringDiagnostic[];
    errors: string[];
};

export type AuthoringRepairContext = {
    ok: boolean;
    diagnostics: AuthoringDiagnostic[];
    errors: string[];
    retryable: boolean;
};

export function requireString(value: unknown, field: string): string {
    if (typeof value !== "string") {
        throw new Error(`Field '${field}' must be a string.`);
    }
    return value;
}

export function requireNonEmptyString(value: unknown, field: string): string {
    if (typeof value !== "string" || value.length === 0) {
        throw new Error(`Plan field '${field}' must be a non-empty string.`);
    }
    return value;
}

export function requireFiniteNumber(value: unknown, field: string): number {
    if (typeof value !== "number" || !Number.isFinite(value)) {
        throw new Error(`Plan field '${field}' must be a finite number.`);
    }
    return value;
}

export function requireNonNegativeInteger(value: unknown, field: string): number {
    const number = requireFiniteNumber(value, field);
    if (!Number.isInteger(number) || number < 0) {
        throw new Error(`Plan field '${field}' must be a non-negative integer.`);
    }
    return number;
}

export function requireVec3(value: unknown, field: string): Vec3 {
    if (!Array.isArray(value) || value.length !== 3) {
        throw new Error(`Plan field '${field}' must be a 3-number array.`);
    }
    return [
        requireFiniteNumber(value[0], `${field}[0]`),
        requireFiniteNumber(value[1], `${field}[1]`),
        requireFiniteNumber(value[2], `${field}[2]`),
    ];
}

function requireObjectRecord(value: unknown, field: string): Record<string, unknown> {
    if (typeof value !== "object" || value == null || Array.isArray(value)) {
        throw new Error(`Field '${field}' must be an object.`);
    }
    return value as Record<string, unknown>;
}

function requireArray(value: unknown, field: string): unknown[] {
    if (!Array.isArray(value)) {
        throw new Error(`Field '${field}' must be an array.`);
    }
    return value;
}

function requireStringEnum<T extends string>(value: unknown, field: string, allowed: readonly T[]): T {
    const text = requireString(value, field);
    if (!allowed.includes(text as T)) {
        throw new Error(`Field '${field}' must be one of: ${allowed.join(", ")}.`);
    }
    return text as T;
}

function requireNullableStringArray(value: unknown, field: string): Array<string | null> {
    if (!Array.isArray(value) || !value.every((item) => item == null || typeof item === "string")) {
        throw new Error(`Field '${field}' must be a nullable string array.`);
    }
    return [...value] as Array<string | null>;
}

function hasOwn(record: Record<string, unknown>, key: string): boolean {
    return Object.prototype.hasOwnProperty.call(record, key);
}

function rejectUnknownFields(record: Record<string, unknown>, field: string, allowedFields: readonly string[]): void {
    const allowed = new Set(allowedFields);
    for (const key of Object.keys(record)) {
        if (!allowed.has(key)) {
            throw new Error(`Plan field '${field.length === 0 ? key : `${field}.${key}`}' is not supported.`);
        }
    }
}

export function normalizeAuthoringPlanCommand(input: unknown, index: number): PlanCommand {
    if (typeof input !== "object" || input == null || Array.isArray(input)) {
        throw new Error(`Plan command ${index} must be an object.`);
    }

    const record = input as Record<string, unknown>;
    const op = requireNonEmptyString(record.op, `commands[${index}].op`);

    switch (op) {
        case "new":
        case "summary":
        case "snapshot":
            rejectUnknownFields(record, `commands[${index}]`, ["op"]);
            return { op };
        case "open":
        case "save":
            rejectUnknownFields(record, `commands[${index}]`, ["op", "path"]);
            return { op, path: requireNonEmptyString(record.path, `commands[${index}].path`) };
        case "entity":
            rejectUnknownFields(record, `commands[${index}]`, ["op", "alias", "name"]);
            return {
                op,
                alias: requireNonEmptyString(record.alias, `commands[${index}].alias`),
                name: requireNonEmptyString(record.name, `commands[${index}].name`),
            };
        case "camera":
        case "directional-light":
        case "point-light":
        case "spot-light":
            rejectUnknownFields(record, `commands[${index}]`, ["op", "alias"]);
            return { op, alias: requireNonEmptyString(record.alias, `commands[${index}].alias`) };
        case "primitive":
            rejectUnknownFields(record, `commands[${index}]`, ["op", "alias", "mesh"]);
            return {
                op,
                alias: requireNonEmptyString(record.alias, `commands[${index}].alias`),
                mesh: requireNonEmptyString(record.mesh, `commands[${index}].mesh`),
            };
        case "parent":
            rejectUnknownFields(record, `commands[${index}]`, ["op", "child", "parent"]);
            return {
                op,
                child: requireNonEmptyString(record.child, `commands[${index}].child`),
                parent: requireNonEmptyString(record.parent, `commands[${index}].parent`),
            };
        case "unparent":
            rejectUnknownFields(record, `commands[${index}]`, ["op", "child"]);
            return { op, child: requireNonEmptyString(record.child, `commands[${index}].child`) };
        case "name":
            rejectUnknownFields(record, `commands[${index}]`, ["op", "entity", "name"]);
            return {
                op,
                entity: requireNonEmptyString(record.entity, `commands[${index}].entity`),
                name: requireNonEmptyString(record.name, `commands[${index}].name`),
            };
        case "transform":
            rejectUnknownFields(record, `commands[${index}]`, ["op", "entity", "translation", "rotationDeg", "scale"]);
            return {
                op,
                entity: requireNonEmptyString(record.entity, `commands[${index}].entity`),
                translation: requireVec3(record.translation, `commands[${index}].translation`),
                rotationDeg: record.rotationDeg == null
                    ? undefined
                    : requireVec3(record.rotationDeg, `commands[${index}].rotationDeg`),
                scale: record.scale == null ? undefined : requireVec3(record.scale, `commands[${index}].scale`),
            };
        case "light-intensity":
            rejectUnknownFields(record, `commands[${index}]`, ["op", "entity", "value"]);
            return {
                op,
                entity: requireNonEmptyString(record.entity, `commands[${index}].entity`),
                value: requireFiniteNumber(record.value, `commands[${index}].value`),
            };
        case "light-color":
            rejectUnknownFields(record, `commands[${index}]`, ["op", "entity", "color"]);
            return {
                op,
                entity: requireNonEmptyString(record.entity, `commands[${index}].entity`),
                color: requireVec3(record.color, `commands[${index}].color`),
            };
        case "camera-perspective":
            rejectUnknownFields(record, `commands[${index}]`, ["op", "entity", "fovDeg", "near", "far"]);
            return {
                op,
                entity: requireNonEmptyString(record.entity, `commands[${index}].entity`),
                fovDeg: requireFiniteNumber(record.fovDeg, `commands[${index}].fovDeg`),
                near: requireFiniteNumber(record.near, `commands[${index}].near`),
                far: requireFiniteNumber(record.far, `commands[${index}].far`),
            };
        case "camera-orthographic":
            rejectUnknownFields(record, `commands[${index}]`, ["op", "entity", "size", "near", "far"]);
            return {
                op,
                entity: requireNonEmptyString(record.entity, `commands[${index}].entity`),
                size: requireFiniteNumber(record.size, `commands[${index}].size`),
                near: requireFiniteNumber(record.near, `commands[${index}].near`),
                far: requireFiniteNumber(record.far, `commands[${index}].far`),
            };
        case "inspect": {
            const target = requireNonEmptyString(record.target, `commands[${index}].target`);
            if (target === "scene" || target === "hierarchy") {
                rejectUnknownFields(record, `commands[${index}]`, ["op", "target"]);
                return { op, target };
            }
            if (target === "entity") {
                rejectUnknownFields(record, `commands[${index}]`, ["op", "target", "entity"]);
                return {
                    op,
                    target,
                    entity: requireNonEmptyString(record.entity, `commands[${index}].entity`),
                };
            }
            throw new Error(`Plan field 'commands[${index}].target' must be scene, hierarchy, or entity.`);
        }
        case "verify": {
            const check = requireNonEmptyString(record.check, `commands[${index}].check`);
            switch (check) {
                case "sceneSaved":
                case "saved":
                    rejectUnknownFields(record, `commands[${index}]`, ["op", "check"]);
                    return { op, check: "sceneSaved" };
                case "entityExists":
                    rejectUnknownFields(record, `commands[${index}]`, ["op", "check", "entity"]);
                    return {
                        op,
                        check,
                        entity: requireNonEmptyString(record.entity, `commands[${index}].entity`),
                    };
                case "hasComponent":
                    rejectUnknownFields(record, `commands[${index}]`, ["op", "check", "entity", "component"]);
                    return {
                        op,
                        check,
                        entity: requireNonEmptyString(record.entity, `commands[${index}].entity`),
                        component: requireNonEmptyString(record.component, `commands[${index}].component`),
                    };
                case "entityCountAtLeast":
                    rejectUnknownFields(record, `commands[${index}]`, ["op", "check", "count"]);
                    return {
                        op,
                        check,
                        count: requireNonNegativeInteger(record.count, `commands[${index}].count`),
                    };
                default:
                    throw new Error(`Plan field 'commands[${index}].check' is not a supported verify check.`);
            }
        }
        default:
            throw new Error(`Unsupported plan op '${op}'.`);
    }
}

export function normalizeAuthoringPlan(input: unknown): AuthoringPlan {
    if (typeof input !== "object" || input == null || Array.isArray(input)) {
        throw new Error("Plan root must be an object.");
    }

    const record = input as Record<string, unknown>;
    rejectUnknownFields(record, "", ["protocol", "project", "commands"]);
    if (record.protocol != null) {
        if (typeof record.protocol !== "object" || Array.isArray(record.protocol)) {
            throw new Error("Plan field 'protocol' must be an object.");
        }

        const protocol = record.protocol as Record<string, unknown>;
        rejectUnknownFields(protocol, "protocol", ["name", "version"]);
        if (protocol.name !== AUTHORING_PROTOCOL_NAME || protocol.version !== AUTHORING_PROTOCOL_VERSION) {
            throw new Error("Unsupported Luna authoring protocol.");
        }
    }

    if (!Array.isArray(record.commands)) {
        throw new Error("Plan field 'commands' must be an array.");
    }

    return {
        protocol: { name: AUTHORING_PROTOCOL_NAME, version: AUTHORING_PROTOCOL_VERSION },
        project: record.project == null ? undefined : requireNonEmptyString(record.project, "project"),
        commands: record.commands.map((command, index) => normalizeAuthoringPlanCommand(command, index)),
    };
}

export function readAuthoringPlanJson(planPath: string): AuthoringPlan {
    return normalizeAuthoringPlan(JSON.parse(readFileSync(planPath, "utf8")) as unknown);
}

export function writeAuthoringPlanJson(plan: AuthoringPlan): string {
    return `${JSON.stringify(normalizeAuthoringPlan(plan), null, 2)}\n`;
}

function requireBoolean(value: unknown, field: string): boolean {
    if (typeof value !== "boolean") {
        throw new Error(`Capability field '${field}' must be a boolean.`);
    }
    return value;
}

function requireStringArray(value: unknown, field: string): string[] {
    if (!Array.isArray(value) || !value.every((item) => typeof item === "string")) {
        throw new Error(`Field '${field}' must be a string array.`);
    }
    return [...value];
}

function normalizeCapabilityParameter(input: unknown, field: string): AuthoringCapabilityParameter {
    if (typeof input !== "object" || input == null || Array.isArray(input)) {
        throw new Error(`Capability field '${field}' must be an object.`);
    }

    const record = input as Record<string, unknown>;
    const type = requireNonEmptyString(record.type, `${field}.type`);
    if (type !== "string" && type !== "number" && type !== "integer" && type !== "boolean" && type !== "vec3") {
        throw new Error(`Capability field '${field}.type' is not a supported parameter type.`);
    }

    return {
        type,
        required: requireBoolean(record.required, `${field}.required`),
        description: requireNonEmptyString(record.description, `${field}.description`),
        enum: record.enum == null ? undefined : requireStringArray(record.enum, `${field}.enum`),
    };
}

function normalizeCapabilityExample(input: unknown, field: string): AuthoringCapabilityExample {
    if (typeof input !== "object" || input == null || Array.isArray(input)) {
        throw new Error(`Capability field '${field}' must be an object.`);
    }

    const record = input as Record<string, unknown>;
    return {
        description: requireNonEmptyString(record.description, `${field}.description`),
        command: typeof record.command === "string"
            ? record.command
            : normalizeAuthoringPlanCommand(record.command, 0),
    };
}

function normalizeCapability(input: unknown, index: number): AuthoringCapability {
    if (typeof input !== "object" || input == null || Array.isArray(input)) {
        throw new Error(`Capability ${index} must be an object.`);
    }

    const record = input as Record<string, unknown>;
    const parameter_record = record.parameters;
    if (typeof parameter_record !== "object" || parameter_record == null || Array.isArray(parameter_record)) {
        throw new Error(`Capability field 'capabilities[${index}].parameters' must be an object.`);
    }
    const parameters: Record<string, AuthoringCapabilityParameter> = {};
    for (const [name, parameter] of Object.entries(parameter_record as Record<string, unknown>)) {
        parameters[name] = normalizeCapabilityParameter(parameter, `capabilities[${index}].parameters.${name}`);
    }

    const examples = record.examples;
    if (!Array.isArray(examples)) {
        throw new Error(`Capability field 'capabilities[${index}].examples' must be an array.`);
    }

    return {
        op: requireNonEmptyString(record.op, `capabilities[${index}].op`),
        title: requireNonEmptyString(record.title, `capabilities[${index}].title`),
        description: requireNonEmptyString(record.description, `capabilities[${index}].description`),
        effects: requireStringArray(record.effects, `capabilities[${index}].effects`),
        requiresConfirmation: requireBoolean(record.requiresConfirmation, `capabilities[${index}].requiresConfirmation`),
        parameters,
        examples: examples.map((example, exampleIndex) =>
            normalizeCapabilityExample(example, `capabilities[${index}].examples[${exampleIndex}]`)
        ),
    };
}

export function normalizeAuthoringCapabilities(input: unknown): AuthoringCapabilitiesDocument {
    if (typeof input !== "object" || input == null || Array.isArray(input)) {
        throw new Error("Capabilities root must be an object.");
    }

    const record = input as Record<string, unknown>;
    if (typeof record.protocol !== "object" || record.protocol == null || Array.isArray(record.protocol)) {
        throw new Error("Capabilities field 'protocol' must be an object.");
    }
    const protocol = record.protocol as Record<string, unknown>;
    if (protocol.name !== AUTHORING_PROTOCOL_NAME || protocol.version !== AUTHORING_PROTOCOL_VERSION) {
        throw new Error("Unsupported Luna authoring protocol.");
    }

    if (!Array.isArray(record.capabilities)) {
        throw new Error("Capabilities field 'capabilities' must be an array.");
    }

    return {
        protocol: { name: AUTHORING_PROTOCOL_NAME, version: AUTHORING_PROTOCOL_VERSION },
        capabilities: record.capabilities.map((capability, index) => normalizeCapability(capability, index)),
    };
}

function nullableString(value: unknown, field: string): string | null {
    if (value == null) {
        return null;
    }
    return requireNonEmptyString(value, field);
}

function nullableInteger(value: unknown, field: string): number | null {
    if (value == null) {
        return null;
    }
    return requireNonNegativeInteger(value, field);
}

function normalizeSceneSnapshot(input: unknown): AuthoringSceneSnapshot {
    const record = requireObjectRecord(input, "scene");
    return {
        name: requireString(record.name, "scene.name"),
        path: nullableString(record.path, "scene.path"),
        entityCount: requireNonNegativeInteger(record.entityCount, "scene.entityCount"),
        dirty: requireBoolean(record.dirty, "scene.dirty"),
    };
}

function normalizeEntityBinding(input: unknown, index: number): AuthoringEntityBinding {
    const record = requireObjectRecord(input, `entities[${index}]`);
    return {
        alias: requireString(record.alias, `entities[${index}].alias`),
        uuid: requireNonEmptyString(record.uuid, `entities[${index}].uuid`),
        name: requireString(record.name, `entities[${index}].name`),
    };
}

function normalizeTransformInspection(input: unknown, field: string): AuthoringTransformInspection {
    const record = requireObjectRecord(input, field);
    return {
        translation: requireVec3(record.translation, `${field}.translation`),
        rotationDeg: requireVec3(record.rotationDeg, `${field}.rotationDeg`),
        scale: requireVec3(record.scale, `${field}.scale`),
    };
}

function normalizeCameraInspection(input: unknown, field: string): AuthoringCameraInspection {
    const record = requireObjectRecord(input, field);
    return {
        primary: requireBoolean(record.primary, `${field}.primary`),
        fixedAspectRatio: requireBoolean(record.fixedAspectRatio, `${field}.fixedAspectRatio`),
        projection: requireString(record.projection, `${field}.projection`),
        perspectiveFovDeg: requireFiniteNumber(record.perspectiveFovDeg, `${field}.perspectiveFovDeg`),
        perspectiveNear: requireFiniteNumber(record.perspectiveNear, `${field}.perspectiveNear`),
        perspectiveFar: requireFiniteNumber(record.perspectiveFar, `${field}.perspectiveFar`),
        orthographicSize: requireFiniteNumber(record.orthographicSize, `${field}.orthographicSize`),
        orthographicNear: requireFiniteNumber(record.orthographicNear, `${field}.orthographicNear`),
        orthographicFar: requireFiniteNumber(record.orthographicFar, `${field}.orthographicFar`),
    };
}

function normalizeLightInspection(input: unknown, field: string): AuthoringLightInspection {
    const record = requireObjectRecord(input, field);
    return {
        type: requireString(record.type, `${field}.type`),
        enabled: requireBoolean(record.enabled, `${field}.enabled`),
        color: requireVec3(record.color, `${field}.color`),
        intensity: requireFiniteNumber(record.intensity, `${field}.intensity`),
        range: requireFiniteNumber(record.range, `${field}.range`),
        innerConeAngleDeg: requireFiniteNumber(record.innerConeAngleDeg, `${field}.innerConeAngleDeg`),
        outerConeAngleDeg: requireFiniteNumber(record.outerConeAngleDeg, `${field}.outerConeAngleDeg`),
    };
}

function normalizeMeshInspection(input: unknown, field: string): AuthoringMeshInspection {
    const record = requireObjectRecord(input, field);
    return {
        meshHandle: nullableString(record.meshHandle, `${field}.meshHandle`),
        firstSubmesh: hasOwn(record, "firstSubmesh")
            ? requireNonNegativeInteger(record.firstSubmesh, `${field}.firstSubmesh`)
            : undefined,
        submeshCount: hasOwn(record, "submeshCount")
            ? requireNonNegativeInteger(record.submeshCount, `${field}.submeshCount`)
            : undefined,
        submeshMaterials: requireNullableStringArray(record.submeshMaterials, `${field}.submeshMaterials`),
    };
}

function normalizeEntityInspection(input: unknown, index: number): AuthoringEntityInspection {
    const field = `inspections[].entities[${index}]`;
    const record = requireObjectRecord(input, field);
    return {
        ref: requireString(record.ref, `${field}.ref`),
        uuid: nullableString(record.uuid, `${field}.uuid`),
        name: requireString(record.name, `${field}.name`),
        parentUuid: nullableString(record.parentUuid, `${field}.parentUuid`),
        children: requireNullableStringArray(record.children, `${field}.children`),
        components: requireStringArray(record.components, `${field}.components`),
        transform: hasOwn(record, "transform")
            ? normalizeTransformInspection(record.transform, `${field}.transform`)
            : undefined,
        camera: hasOwn(record, "camera") ? normalizeCameraInspection(record.camera, `${field}.camera`) : undefined,
        light: hasOwn(record, "light") ? normalizeLightInspection(record.light, `${field}.light`) : undefined,
        mesh: hasOwn(record, "mesh") ? normalizeMeshInspection(record.mesh, `${field}.mesh`) : undefined,
    };
}

function normalizeInspection(input: unknown, index: number): AuthoringInspection {
    const field = `inspections[${index}]`;
    const record = requireObjectRecord(input, field);
    const entities = requireArray(record.entities, `${field}.entities`);
    return {
        type: requireStringEnum(record.type, `${field}.type`, ["scene", "entity", "hierarchy"]),
        ref: requireString(record.ref, `${field}.ref`),
        entities: entities.map((entity, entityIndex) => normalizeEntityInspection(entity, entityIndex)),
    };
}

function normalizeVerification(input: unknown, index: number): AuthoringVerification {
    const field = `verifications[${index}]`;
    const record = requireObjectRecord(input, field);
    return {
        type: requireStringEnum(record.type, `${field}.type`, [
            "sceneSaved",
            "entityExists",
            "hasComponent",
            "entityCountAtLeast",
        ]),
        ok: requireBoolean(record.ok, `${field}.ok`),
        ref: requireString(record.ref, `${field}.ref`),
        uuid: nullableString(record.uuid, `${field}.uuid`),
        component: requireString(record.component, `${field}.component`),
        expectedCount: requireNonNegativeInteger(record.expectedCount, `${field}.expectedCount`),
        actualCount: requireNonNegativeInteger(record.actualCount, `${field}.actualCount`),
        message: requireString(record.message, `${field}.message`),
    };
}

function normalizeDiagnostic(input: unknown, index: number): AuthoringDiagnostic {
    if (typeof input !== "object" || input == null || Array.isArray(input)) {
        throw new Error(`Report diagnostic ${index} must be an object.`);
    }

    const record = input as Record<string, unknown>;
    return {
        severity: requireStringEnum(record.severity, `diagnostics[${index}].severity`, ["info", "warning", "error"]),
        phase: requireStringEnum(record.phase, `diagnostics[${index}].phase`, ["parse", "validate", "execute", "verify"]),
        code: requireNonEmptyString(record.code, `diagnostics[${index}].code`),
        message: requireNonEmptyString(record.message, `diagnostics[${index}].message`),
        commandIndex: nullableInteger(record.commandIndex, `diagnostics[${index}].commandIndex`),
        command: nullableString(record.command, `diagnostics[${index}].command`),
        field: nullableString(record.field, `diagnostics[${index}].field`),
        entityRef: nullableString(record.entityRef, `diagnostics[${index}].entityRef`),
        component: nullableString(record.component, `diagnostics[${index}].component`),
        path: nullableString(record.path, `diagnostics[${index}].path`),
        recoverable: requireBoolean(record.recoverable, `diagnostics[${index}].recoverable`),
        expected: nullableString(record.expected, `diagnostics[${index}].expected`),
        actual: nullableString(record.actual, `diagnostics[${index}].actual`),
        suggestedCommand: nullableString(record.suggestedCommand, `diagnostics[${index}].suggestedCommand`),
    };
}

export function normalizeAuthoringReport(input: unknown): AuthoringReport {
    if (typeof input !== "object" || input == null || Array.isArray(input)) {
        throw new Error("Report root must be an object.");
    }

    const record = input as Record<string, unknown>;
    if (typeof record.protocol !== "object" || record.protocol == null || Array.isArray(record.protocol)) {
        throw new Error("Report field 'protocol' must be an object.");
    }
    const protocol = record.protocol as Record<string, unknown>;
    if (protocol.name !== AUTHORING_PROTOCOL_NAME || protocol.version !== AUTHORING_PROTOCOL_VERSION) {
        throw new Error("Unsupported Luna authoring protocol.");
    }

    const entities = requireArray(record.entities, "entities");
    const inspections = requireArray(record.inspections, "inspections");
    const verifications = requireArray(record.verifications, "verifications");
    const diagnostics = requireArray(record.diagnostics, "diagnostics");

    return {
        protocol: { name: AUTHORING_PROTOCOL_NAME, version: AUTHORING_PROTOCOL_VERSION },
        ok: requireBoolean(record.ok, "ok"),
        scene: normalizeSceneSnapshot(record.scene),
        entities: entities.map((entity, index) => normalizeEntityBinding(entity, index)),
        savedScenes: requireStringArray(record.savedScenes, "savedScenes"),
        inspections: inspections.map((inspection, index) => normalizeInspection(inspection, index)),
        verifications: verifications.map((verification, index) => normalizeVerification(verification, index)),
        diagnostics: diagnostics.map((diagnostic, index) => normalizeDiagnostic(diagnostic, index)),
        errors: requireStringArray(record.errors, "errors"),
    };
}

export function latestInspection(
    report: AuthoringReport,
    type?: AuthoringInspection["type"],
): AuthoringInspection | undefined {
    for (let index = report.inspections.length - 1; index >= 0; --index) {
        const inspection = report.inspections[index];
        if (inspection != null && (type == null || inspection.type === type)) {
            return inspection;
        }
    }
    return undefined;
}

export function latestHierarchy(report: AuthoringReport): AuthoringInspection | undefined {
    return latestInspection(report, "hierarchy");
}

export function latestHierarchyEntities(report: AuthoringReport): AuthoringEntityInspection[] {
    return latestHierarchy(report)?.entities ?? [];
}

export function findEntityBinding(report: AuthoringReport, ref: string): AuthoringEntityBinding | undefined {
    return report.entities.find((binding) => binding.alias === ref || binding.uuid === ref);
}

function inspectedEntitiesNewestFirst(report: AuthoringReport): AuthoringEntityInspection[] {
    const entities: AuthoringEntityInspection[] = [];
    for (let inspectionIndex = report.inspections.length - 1; inspectionIndex >= 0; --inspectionIndex) {
        const inspection = report.inspections[inspectionIndex];
        if (inspection == null) {
            continue;
        }
        entities.push(...inspection.entities);
    }
    return entities;
}

function preferredEntitySearchScope(report: AuthoringReport): AuthoringEntityInspection[] {
    const hierarchyEntities = latestHierarchyEntities(report);
    return hierarchyEntities.length > 0 ? hierarchyEntities : inspectedEntitiesNewestFirst(report);
}

export function findEntityByUuid(report: AuthoringReport, uuid: string): AuthoringEntityInspection | undefined {
    if (uuid.length === 0) {
        return undefined;
    }
    return preferredEntitySearchScope(report).find((entity) => entity.uuid === uuid);
}

export function findEntityByRef(report: AuthoringReport, ref: string): AuthoringEntityInspection | undefined {
    if (ref.length === 0) {
        return undefined;
    }

    const binding = findEntityBinding(report, ref);
    if (binding != null) {
        const boundEntity = findEntityByUuid(report, binding.uuid);
        if (boundEntity != null) {
            return boundEntity;
        }
    }

    return preferredEntitySearchScope(report).find((entity) => entity.ref === ref || entity.uuid === ref);
}

export function findEntityByName(report: AuthoringReport, name: string): AuthoringEntityInspection | undefined {
    if (name.length === 0) {
        return undefined;
    }
    return preferredEntitySearchScope(report).find((entity) => entity.name === name);
}

export function findEntitiesByComponent(report: AuthoringReport, component: string): AuthoringEntityInspection[] {
    if (component.length === 0) {
        return [];
    }
    return preferredEntitySearchScope(report).filter((entity) => entity.components.includes(component));
}

export function buildAuthoringRepairContext(report: AuthoringReport): AuthoringRepairContext {
    return {
        ok: report.ok,
        diagnostics: report.diagnostics,
        errors: report.errors,
        retryable: report.diagnostics.some((diagnostic) => diagnostic.recoverable),
    };
}

export class AuthoringPlanBuilder {
    #project?: string;
    #commands: PlanCommand[] = [];

    project(path: string): this {
        this.#project = requireNonEmptyString(path, "project");
        return this;
    }

    newScene(): this {
        this.#commands.push({ op: "new" });
        return this;
    }

    openScene(path: string): this {
        this.#commands.push({ op: "open", path: requireNonEmptyString(path, "path") });
        return this;
    }

    saveScene(path: string): this {
        this.#commands.push({ op: "save", path: requireNonEmptyString(path, "path") });
        return this;
    }

    entity(alias: string, name: string): this {
        this.#commands.push({
            op: "entity",
            alias: requireNonEmptyString(alias, "alias"),
            name: requireNonEmptyString(name, "name"),
        });
        return this;
    }

    camera(alias: string): this {
        this.#commands.push({ op: "camera", alias: requireNonEmptyString(alias, "alias") });
        return this;
    }

    directionalLight(alias: string): this {
        this.#commands.push({ op: "directional-light", alias: requireNonEmptyString(alias, "alias") });
        return this;
    }

    pointLight(alias: string): this {
        this.#commands.push({ op: "point-light", alias: requireNonEmptyString(alias, "alias") });
        return this;
    }

    spotLight(alias: string): this {
        this.#commands.push({ op: "spot-light", alias: requireNonEmptyString(alias, "alias") });
        return this;
    }

    primitive(alias: string, mesh: string): this {
        this.#commands.push({
            op: "primitive",
            alias: requireNonEmptyString(alias, "alias"),
            mesh: requireNonEmptyString(mesh, "mesh"),
        });
        return this;
    }

    parent(child: string, parent: string): this {
        this.#commands.push({
            op: "parent",
            child: requireNonEmptyString(child, "child"),
            parent: requireNonEmptyString(parent, "parent"),
        });
        return this;
    }

    unparent(child: string): this {
        this.#commands.push({ op: "unparent", child: requireNonEmptyString(child, "child") });
        return this;
    }

    rename(entity: string, name: string): this {
        this.#commands.push({
            op: "name",
            entity: requireNonEmptyString(entity, "entity"),
            name: requireNonEmptyString(name, "name"),
        });
        return this;
    }

    transform(entity: string, translation: Vec3, rotationDeg?: Vec3, scale?: Vec3): this {
        this.#commands.push({
            op: "transform",
            entity: requireNonEmptyString(entity, "entity"),
            translation: requireVec3(translation, "translation"),
            rotationDeg: rotationDeg == null ? undefined : requireVec3(rotationDeg, "rotationDeg"),
            scale: scale == null ? undefined : requireVec3(scale, "scale"),
        });
        return this;
    }

    lightIntensity(entity: string, value: number): this {
        this.#commands.push({
            op: "light-intensity",
            entity: requireNonEmptyString(entity, "entity"),
            value: requireFiniteNumber(value, "value"),
        });
        return this;
    }

    lightColor(entity: string, color: Vec3): this {
        this.#commands.push({
            op: "light-color",
            entity: requireNonEmptyString(entity, "entity"),
            color: requireVec3(color, "color"),
        });
        return this;
    }

    cameraPerspective(entity: string, fovDeg: number, near: number, far: number): this {
        this.#commands.push({
            op: "camera-perspective",
            entity: requireNonEmptyString(entity, "entity"),
            fovDeg: requireFiniteNumber(fovDeg, "fovDeg"),
            near: requireFiniteNumber(near, "near"),
            far: requireFiniteNumber(far, "far"),
        });
        return this;
    }

    cameraOrthographic(entity: string, size: number, near: number, far: number): this {
        this.#commands.push({
            op: "camera-orthographic",
            entity: requireNonEmptyString(entity, "entity"),
            size: requireFiniteNumber(size, "size"),
            near: requireFiniteNumber(near, "near"),
            far: requireFiniteNumber(far, "far"),
        });
        return this;
    }

    inspectScene(): this {
        this.#commands.push({ op: "inspect", target: "scene" });
        return this;
    }

    inspectEntity(entity: string): this {
        this.#commands.push({
            op: "inspect",
            target: "entity",
            entity: requireNonEmptyString(entity, "entity"),
        });
        return this;
    }

    inspectHierarchy(): this {
        this.#commands.push({ op: "inspect", target: "hierarchy" });
        return this;
    }

    verifySceneSaved(): this {
        this.#commands.push({ op: "verify", check: "sceneSaved" });
        return this;
    }

    verifyEntityExists(entity: string): this {
        this.#commands.push({
            op: "verify",
            check: "entityExists",
            entity: requireNonEmptyString(entity, "entity"),
        });
        return this;
    }

    verifyHasComponent(entity: string, component: string): this {
        this.#commands.push({
            op: "verify",
            check: "hasComponent",
            entity: requireNonEmptyString(entity, "entity"),
            component: requireNonEmptyString(component, "component"),
        });
        return this;
    }

    verifyEntityCountAtLeast(count: number): this {
        this.#commands.push({
            op: "verify",
            check: "entityCountAtLeast",
            count: requireNonNegativeInteger(count, "count"),
        });
        return this;
    }

    summary(): this {
        this.#commands.push({ op: "summary" });
        return this;
    }

    snapshot(): this {
        this.#commands.push({ op: "snapshot" });
        return this;
    }

    toPlan(): AuthoringPlan {
        return normalizeAuthoringPlan({
            protocol: { name: AUTHORING_PROTOCOL_NAME, version: AUTHORING_PROTOCOL_VERSION },
            project: this.#project,
            commands: this.#commands,
        });
    }
}

function expectTokenCount(tokens: string[], expected: number, usage: string): void {
    if (tokens.length !== expected) {
        throw new Error(`Command '${tokens[0] ?? ""}' expects ${usage}.`);
    }
}

function numberToken(token: string | undefined, field: string): number {
    if (token == null) {
        throw new Error(`Missing numeric argument '${field}'.`);
    }
    return requireFiniteNumber(Number(token), field);
}

function nonNegativeIntegerToken(token: string | undefined, field: string): number {
    if (token == null) {
        throw new Error(`Missing integer argument '${field}'.`);
    }
    return requireNonNegativeInteger(Number(token), field);
}

function vec3Tokens(tokens: string[], startIndex: number, field: string): Vec3 {
    return [
        numberToken(tokens[startIndex], `${field}.x`),
        numberToken(tokens[startIndex + 1], `${field}.y`),
        numberToken(tokens[startIndex + 2], `${field}.z`),
    ];
}

function firstBuiltCommand(builder: AuthoringPlanBuilder): PlanCommand {
    const [command] = builder.toPlan().commands;
    if (command == null) {
        throw new Error("No authoring command was built.");
    }
    return command;
}

export function parseAuthoringCommandTokens(tokens: string[]): PlanCommand {
    const command = tokens[0];
    const builder = new AuthoringPlanBuilder();

    switch (command) {
        case "new":
            expectTokenCount(tokens, 1, "no arguments");
            return firstBuiltCommand(builder.newScene());
        case "summary":
            expectTokenCount(tokens, 1, "no arguments");
            return firstBuiltCommand(builder.summary());
        case "snapshot":
            expectTokenCount(tokens, 1, "no arguments");
            return firstBuiltCommand(builder.snapshot());
        case "open":
            expectTokenCount(tokens, 2, "<scene-path>");
            return firstBuiltCommand(builder.openScene(requireNonEmptyString(tokens[1], "path")));
        case "save":
            expectTokenCount(tokens, 2, "<scene-path>");
            return firstBuiltCommand(builder.saveScene(requireNonEmptyString(tokens[1], "path")));
        case "entity":
            expectTokenCount(tokens, 3, "<alias> <name>");
            return firstBuiltCommand(
                builder.entity(requireNonEmptyString(tokens[1], "alias"), requireNonEmptyString(tokens[2], "name")),
            );
        case "camera":
            expectTokenCount(tokens, 2, "<alias>");
            return firstBuiltCommand(builder.camera(requireNonEmptyString(tokens[1], "alias")));
        case "directional-light":
            expectTokenCount(tokens, 2, "<alias>");
            return firstBuiltCommand(builder.directionalLight(requireNonEmptyString(tokens[1], "alias")));
        case "point-light":
            expectTokenCount(tokens, 2, "<alias>");
            return firstBuiltCommand(builder.pointLight(requireNonEmptyString(tokens[1], "alias")));
        case "spot-light":
            expectTokenCount(tokens, 2, "<alias>");
            return firstBuiltCommand(builder.spotLight(requireNonEmptyString(tokens[1], "alias")));
        case "primitive":
            expectTokenCount(tokens, 3, "<alias> <mesh>");
            return firstBuiltCommand(
                builder.primitive(requireNonEmptyString(tokens[1], "alias"), requireNonEmptyString(tokens[2], "mesh")),
            );
        case "parent":
            expectTokenCount(tokens, 3, "<child-ref> <parent-ref>");
            return firstBuiltCommand(
                builder.parent(requireNonEmptyString(tokens[1], "child"), requireNonEmptyString(tokens[2], "parent")),
            );
        case "unparent":
            expectTokenCount(tokens, 2, "<child-ref>");
            return firstBuiltCommand(builder.unparent(requireNonEmptyString(tokens[1], "child")));
        case "name":
            expectTokenCount(tokens, 3, "<entity-ref> <name>");
            return firstBuiltCommand(
                builder.rename(requireNonEmptyString(tokens[1], "entity"), requireNonEmptyString(tokens[2], "name")),
            );
        case "transform":
            expectTokenCount(tokens, 11, "<entity-ref> tx ty tz rxDeg ryDeg rzDeg sx sy sz");
            return firstBuiltCommand(
                builder.transform(
                    requireNonEmptyString(tokens[1], "entity"),
                    vec3Tokens(tokens, 2, "translation"),
                    vec3Tokens(tokens, 5, "rotationDeg"),
                    vec3Tokens(tokens, 8, "scale"),
                ),
            );
        case "light-intensity":
            expectTokenCount(tokens, 3, "<entity-ref> <value>");
            return firstBuiltCommand(
                builder.lightIntensity(
                    requireNonEmptyString(tokens[1], "entity"),
                    numberToken(tokens[2], "value"),
                ),
            );
        case "light-color":
            expectTokenCount(tokens, 5, "<entity-ref> r g b");
            return firstBuiltCommand(
                builder.lightColor(requireNonEmptyString(tokens[1], "entity"), vec3Tokens(tokens, 2, "color")),
            );
        case "camera-perspective":
            expectTokenCount(tokens, 5, "<entity-ref> fovDeg near far");
            return firstBuiltCommand(
                builder.cameraPerspective(
                    requireNonEmptyString(tokens[1], "entity"),
                    numberToken(tokens[2], "fovDeg"),
                    numberToken(tokens[3], "near"),
                    numberToken(tokens[4], "far"),
                ),
            );
        case "camera-orthographic":
            expectTokenCount(tokens, 5, "<entity-ref> size near far");
            return firstBuiltCommand(
                builder.cameraOrthographic(
                    requireNonEmptyString(tokens[1], "entity"),
                    numberToken(tokens[2], "size"),
                    numberToken(tokens[3], "near"),
                    numberToken(tokens[4], "far"),
                ),
            );
        case "inspect": {
            const target = tokens[1];
            if (target === "scene") {
                expectTokenCount(tokens, 2, "scene");
                return firstBuiltCommand(builder.inspectScene());
            }
            if (target === "hierarchy") {
                expectTokenCount(tokens, 2, "hierarchy");
                return firstBuiltCommand(builder.inspectHierarchy());
            }
            if (target === "entity") {
                expectTokenCount(tokens, 3, "entity <entity-ref>");
                return firstBuiltCommand(builder.inspectEntity(requireNonEmptyString(tokens[2], "entity")));
            }
            throw new Error("Command 'inspect' expects scene, hierarchy, or entity.");
        }
        case "verify": {
            const check = tokens[1];
            if (check === "saved" || check === "sceneSaved") {
                expectTokenCount(tokens, 2, "saved");
                return firstBuiltCommand(builder.verifySceneSaved());
            }
            if (check === "entity" || check === "entityExists") {
                expectTokenCount(tokens, 3, "entity <entity-ref>");
                return firstBuiltCommand(builder.verifyEntityExists(requireNonEmptyString(tokens[2], "entity")));
            }
            if (check === "component" || check === "hasComponent") {
                expectTokenCount(tokens, 4, "component <entity-ref> <component>");
                return firstBuiltCommand(
                    builder.verifyHasComponent(
                        requireNonEmptyString(tokens[2], "entity"),
                        requireNonEmptyString(tokens[3], "component"),
                    ),
                );
            }
            if (check === "entity-count-at-least" || check === "entityCountAtLeast") {
                expectTokenCount(tokens, 3, "entity-count-at-least <count>");
                return firstBuiltCommand(builder.verifyEntityCountAtLeast(nonNegativeIntegerToken(tokens[2], "count")));
            }
            throw new Error("Command 'verify' expects saved, entity, component, or entity-count-at-least.");
        }
        default:
            throw new Error(`Unsupported authoring command '${command ?? ""}'.`);
    }
}
