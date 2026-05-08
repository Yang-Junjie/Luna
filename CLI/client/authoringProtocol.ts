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
    | { op: "summary" };

export type AuthoringPlan = {
    protocol?: AuthoringProtocolInfo;
    project?: string;
    commands: PlanCommand[];
};

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

export function normalizeAuthoringPlanCommand(input: unknown, index: number): PlanCommand {
    if (typeof input !== "object" || input == null || Array.isArray(input)) {
        throw new Error(`Plan command ${index} must be an object.`);
    }

    const record = input as Record<string, unknown>;
    const op = requireNonEmptyString(record.op, `commands[${index}].op`);

    switch (op) {
        case "new":
        case "summary":
            return { op };
        case "open":
        case "save":
            return { op, path: requireNonEmptyString(record.path, `commands[${index}].path`) };
        case "entity":
            return {
                op,
                alias: requireNonEmptyString(record.alias, `commands[${index}].alias`),
                name: requireNonEmptyString(record.name, `commands[${index}].name`),
            };
        case "camera":
        case "directional-light":
        case "point-light":
        case "spot-light":
            return { op, alias: requireNonEmptyString(record.alias, `commands[${index}].alias`) };
        case "primitive":
            return {
                op,
                alias: requireNonEmptyString(record.alias, `commands[${index}].alias`),
                mesh: requireNonEmptyString(record.mesh, `commands[${index}].mesh`),
            };
        case "parent":
            return {
                op,
                child: requireNonEmptyString(record.child, `commands[${index}].child`),
                parent: requireNonEmptyString(record.parent, `commands[${index}].parent`),
            };
        case "unparent":
            return { op, child: requireNonEmptyString(record.child, `commands[${index}].child`) };
        case "name":
            return {
                op,
                entity: requireNonEmptyString(record.entity, `commands[${index}].entity`),
                name: requireNonEmptyString(record.name, `commands[${index}].name`),
            };
        case "transform":
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
            return {
                op,
                entity: requireNonEmptyString(record.entity, `commands[${index}].entity`),
                value: requireFiniteNumber(record.value, `commands[${index}].value`),
            };
        case "light-color":
            return {
                op,
                entity: requireNonEmptyString(record.entity, `commands[${index}].entity`),
                color: requireVec3(record.color, `commands[${index}].color`),
            };
        case "camera-perspective":
            return {
                op,
                entity: requireNonEmptyString(record.entity, `commands[${index}].entity`),
                fovDeg: requireFiniteNumber(record.fovDeg, `commands[${index}].fovDeg`),
                near: requireFiniteNumber(record.near, `commands[${index}].near`),
                far: requireFiniteNumber(record.far, `commands[${index}].far`),
            };
        case "camera-orthographic":
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
                return { op, target };
            }
            if (target === "entity") {
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
                    return { op, check: "sceneSaved" };
                case "entityExists":
                    return {
                        op,
                        check,
                        entity: requireNonEmptyString(record.entity, `commands[${index}].entity`),
                    };
                case "hasComponent":
                    return {
                        op,
                        check,
                        entity: requireNonEmptyString(record.entity, `commands[${index}].entity`),
                        component: requireNonEmptyString(record.component, `commands[${index}].component`),
                    };
                case "entityCountAtLeast":
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
    if (record.protocol != null) {
        if (typeof record.protocol !== "object" || Array.isArray(record.protocol)) {
            throw new Error("Plan field 'protocol' must be an object.");
        }

        const protocol = record.protocol as Record<string, unknown>;
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
