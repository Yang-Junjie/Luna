#!/usr/bin/env node

import { spawnSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { createInterface } from "node:readline/promises";
import { stdin as input, stdout as output } from "node:process";
import { fileURLToPath } from "node:url";

type Vec3 = [number, number, number];

type PlanCommand =
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
    | { op: "inspect"; target: "scene" | "hierarchy" | "entity"; entity?: string }
    | { op: "verify"; check: "sceneSaved" }
    | { op: "verify"; check: "entityExists"; entity: string }
    | { op: "verify"; check: "hasComponent"; entity: string; component: string }
    | { op: "verify"; check: "entityCountAtLeast"; count: number }
    | { op: "summary" };

type AuthoringPlan = {
    protocol?: {
        name: "luna.authoring";
        version: 1;
    };
    project?: string;
    commands: PlanCommand[];
};

type ClientOptions = {
    host?: string;
    project?: string;
    jsonOutput: boolean;
    dryRun: boolean;
    passthrough: string[];
};

function printUsage(): void {
    console.log(`Luna TS CLI

Usage:
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts [--host <path>] [--project <path>] [--json] run <commands...>
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts [--host <path>] [--project <path>] [--json] plan <plan.json>
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts [--host <path>] [--json] interactive

Examples:
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts run new primitive CubeBox Cube save build/CLI/Smoke/TsScene
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts plan CLI/client/examples/smoke.plan.json
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts interactive

The TypeScript client delegates execution to the C++ LunaCLI host.`);
}

function takeOptionValue(args: string[], index: number, option: string): string {
    const value = args[index + 1];
    if (value == null || value.startsWith("--")) {
        throw new Error(`Missing value for ${option}`);
    }
    return value;
}

function parseClientOptions(args: string[]): ClientOptions {
    const options: ClientOptions = { jsonOutput: false, dryRun: false, passthrough: [] };

    for (let index = 0; index < args.length;) {
        const arg = args[index];
        if (arg === "--host") {
            options.host = takeOptionValue(args, index, arg);
            index += 2;
            continue;
        }
        if (arg.startsWith("--host=")) {
            options.host = arg.slice("--host=".length);
            index += 1;
            continue;
        }
        if (arg === "--project") {
            options.project = takeOptionValue(args, index, arg);
            index += 2;
            continue;
        }
        if (arg.startsWith("--project=")) {
            options.project = arg.slice("--project=".length);
            index += 1;
            continue;
        }
        if (arg === "--dry-run") {
            options.dryRun = true;
            index += 1;
            continue;
        }
        if (arg === "--json") {
            options.jsonOutput = true;
            index += 1;
            continue;
        }

        options.passthrough = args.slice(index);
        return options;
    }

    return options;
}

function findDefaultHost(): string {
    const scriptDir = dirname(fileURLToPath(import.meta.url));
    const repoRoot = resolve(scriptDir, "..", "..");
    const candidates = [
        resolve(repoRoot, "build", "CLI", "LunaCLI.exe"),
        resolve(repoRoot, "build", "CLI", "LunaCLI"),
        resolve(process.cwd(), "build", "CLI", "LunaCLI.exe"),
        resolve(process.cwd(), "build", "CLI", "LunaCLI"),
    ];

    for (const candidate of candidates) {
        if (existsSync(candidate)) {
            return candidate;
        }
    }

    throw new Error("Could not find LunaCLI host. Build it first or pass --host <path>.");
}

function requireString(value: unknown, field: string): string {
    if (typeof value !== "string" || value.length === 0) {
        throw new Error(`Plan field '${field}' must be a non-empty string.`);
    }
    return value;
}

function requireNumber(value: unknown, field: string): number {
    if (typeof value !== "number" || !Number.isFinite(value)) {
        throw new Error(`Plan field '${field}' must be a finite number.`);
    }
    return value;
}

function requireNonNegativeInteger(value: unknown, field: string): number {
    const number = requireNumber(value, field);
    if (!Number.isInteger(number) || number < 0) {
        throw new Error(`Plan field '${field}' must be a non-negative integer.`);
    }
    return number;
}

function requireVec3(value: unknown, field: string): Vec3 {
    if (!Array.isArray(value) || value.length !== 3) {
        throw new Error(`Plan field '${field}' must be a 3-number array.`);
    }
    return [
        requireNumber(value[0], `${field}[0]`),
        requireNumber(value[1], `${field}[1]`),
        requireNumber(value[2], `${field}[2]`),
    ];
}

function pushVec3(args: string[], value: Vec3): void {
    args.push(String(value[0]), String(value[1]), String(value[2]));
}

function planCommandToArgs(command: PlanCommand): string[] {
    switch (command.op) {
        case "new":
        case "summary":
            return [command.op];
        case "open":
        case "save":
            return [command.op, command.path];
        case "entity":
            return ["entity", command.alias, command.name];
        case "camera":
        case "directional-light":
        case "point-light":
        case "spot-light":
            return [command.op, command.alias];
        case "primitive":
            return ["primitive", command.alias, command.mesh];
        case "parent":
            return ["parent", command.child, command.parent];
        case "unparent":
            return ["unparent", command.child];
        case "name":
            return ["name", command.entity, command.name];
        case "transform": {
            const args = ["transform", command.entity];
            pushVec3(args, command.translation);
            pushVec3(args, command.rotationDeg ?? [0, 0, 0]);
            pushVec3(args, command.scale ?? [1, 1, 1]);
            return args;
        }
        case "light-intensity":
            return ["light-intensity", command.entity, String(command.value)];
        case "light-color": {
            const args = ["light-color", command.entity];
            pushVec3(args, command.color);
            return args;
        }
        case "camera-perspective":
            return [
                "camera-perspective",
                command.entity,
                String(command.fovDeg),
                String(command.near),
                String(command.far),
            ];
        case "camera-orthographic":
            return [
                "camera-orthographic",
                command.entity,
                String(command.size),
                String(command.near),
                String(command.far),
            ];
        case "inspect":
            return command.target === "entity"
                ? ["inspect", command.target, command.entity ?? ""]
                : ["inspect", command.target];
        case "verify":
            switch (command.check) {
                case "sceneSaved":
                    return ["verify", "saved"];
                case "entityExists":
                    return ["verify", "entity", command.entity];
                case "hasComponent":
                    return ["verify", "component", command.entity, command.component];
                case "entityCountAtLeast":
                    return ["verify", "entity-count-at-least", String(command.count)];
            }
    }
}

function normalizePlanCommand(input: unknown, index: number): PlanCommand {
    if (typeof input !== "object" || input == null || Array.isArray(input)) {
        throw new Error(`Plan command ${index} must be an object.`);
    }

    const record = input as Record<string, unknown>;
    const op = requireString(record.op, `commands[${index}].op`);

    switch (op) {
        case "new":
        case "summary":
            return { op };
        case "open":
        case "save":
            return { op, path: requireString(record.path, `commands[${index}].path`) };
        case "entity":
            return {
                op,
                alias: requireString(record.alias, `commands[${index}].alias`),
                name: requireString(record.name, `commands[${index}].name`),
            };
        case "camera":
        case "directional-light":
        case "point-light":
        case "spot-light":
            return { op, alias: requireString(record.alias, `commands[${index}].alias`) };
        case "primitive":
            return {
                op,
                alias: requireString(record.alias, `commands[${index}].alias`),
                mesh: requireString(record.mesh, `commands[${index}].mesh`),
            };
        case "parent":
            return {
                op,
                child: requireString(record.child, `commands[${index}].child`),
                parent: requireString(record.parent, `commands[${index}].parent`),
            };
        case "unparent":
            return { op, child: requireString(record.child, `commands[${index}].child`) };
        case "name":
            return {
                op,
                entity: requireString(record.entity, `commands[${index}].entity`),
                name: requireString(record.name, `commands[${index}].name`),
            };
        case "transform":
            return {
                op,
                entity: requireString(record.entity, `commands[${index}].entity`),
                translation: requireVec3(record.translation, `commands[${index}].translation`),
                rotationDeg: record.rotationDeg == null
                    ? undefined
                    : requireVec3(record.rotationDeg, `commands[${index}].rotationDeg`),
                scale: record.scale == null ? undefined : requireVec3(record.scale, `commands[${index}].scale`),
            };
        case "light-intensity":
            return {
                op,
                entity: requireString(record.entity, `commands[${index}].entity`),
                value: requireNumber(record.value, `commands[${index}].value`),
            };
        case "light-color":
            return {
                op,
                entity: requireString(record.entity, `commands[${index}].entity`),
                color: requireVec3(record.color, `commands[${index}].color`),
            };
        case "camera-perspective":
            return {
                op,
                entity: requireString(record.entity, `commands[${index}].entity`),
                fovDeg: requireNumber(record.fovDeg, `commands[${index}].fovDeg`),
                near: requireNumber(record.near, `commands[${index}].near`),
                far: requireNumber(record.far, `commands[${index}].far`),
            };
        case "camera-orthographic":
            return {
                op,
                entity: requireString(record.entity, `commands[${index}].entity`),
                size: requireNumber(record.size, `commands[${index}].size`),
                near: requireNumber(record.near, `commands[${index}].near`),
                far: requireNumber(record.far, `commands[${index}].far`),
            };
        case "inspect": {
            const target = requireString(record.target, `commands[${index}].target`);
            if (target !== "scene" && target !== "hierarchy" && target !== "entity") {
                throw new Error(`Plan field 'commands[${index}].target' must be scene, hierarchy, or entity.`);
            }
            return {
                op,
                target,
                entity: target === "entity"
                    ? requireString(record.entity, `commands[${index}].entity`)
                    : undefined,
            };
        }
        case "verify": {
            const check = requireString(record.check, `commands[${index}].check`);
            switch (check) {
                case "sceneSaved":
                    return { op, check };
                case "entityExists":
                    return {
                        op,
                        check,
                        entity: requireString(record.entity, `commands[${index}].entity`),
                    };
                case "hasComponent":
                    return {
                        op,
                        check,
                        entity: requireString(record.entity, `commands[${index}].entity`),
                        component: requireString(record.component, `commands[${index}].component`),
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

function readPlan(planPath: string): AuthoringPlan {
    const raw = JSON.parse(readFileSync(planPath, "utf8")) as unknown;
    if (typeof raw !== "object" || raw == null || Array.isArray(raw)) {
        throw new Error("Plan root must be an object.");
    }

    const record = raw as Record<string, unknown>;
    if (record.protocol != null) {
        if (typeof record.protocol !== "object" || Array.isArray(record.protocol)) {
            throw new Error("Plan field 'protocol' must be an object.");
        }

        const protocol = record.protocol as Record<string, unknown>;
        if (protocol.name !== "luna.authoring" || protocol.version !== 1) {
            throw new Error("Unsupported Luna authoring protocol.");
        }
    }

    if (!Array.isArray(record.commands)) {
        throw new Error("Plan field 'commands' must be an array.");
    }

    return {
        project: record.project == null ? undefined : requireString(record.project, "project"),
        commands: record.commands.map((command, index) => normalizePlanCommand(command, index)),
    };
}

function runHost(host: string, args: string[], dryRun: boolean): number {
    if (dryRun) {
        console.log([host, ...args].map((part) => JSON.stringify(part)).join(" "));
        return 0;
    }

    const result = spawnSync(host, args, { stdio: "inherit" });
    if (typeof result.status === "number") {
        return result.status;
    }
    if (result.error != null) {
        throw result.error;
    }
    return 1;
}

function splitCommandLine(line: string): string[] {
    const tokens: string[] = [];
    let token = "";
    let quote: '"' | "'" | null = null;

    for (let index = 0; index < line.length; ++index) {
        const char = line[index];
        if (quote != null) {
            if (char === quote) {
                quote = null;
            } else {
                token += char;
            }
            continue;
        }

        if (char === '"' || char === "'") {
            quote = char;
            continue;
        }

        if (/\s/.test(char)) {
            if (token.length > 0) {
                tokens.push(token);
                token = "";
            }
            continue;
        }

        token += char;
    }

    if (quote != null) {
        throw new Error("Unclosed quote in command.");
    }
    if (token.length > 0) {
        tokens.push(token);
    }

    return tokens;
}

function printInteractiveHelp(): void {
    console.log(`Interactive commands:
  help                 Show this help
  menu                 Choose commands from a small menu
  show                 Show queued authoring commands
  clear                Clear queued authoring commands
  run                  Execute queued authoring commands
  preview              Execute queued authoring commands plus summary
  exit                 Leave the client

Authoring commands are queued as-is:
  new
  primitive Box Cube
  camera MainCamera
  point-light KeyLight
  transform Box 0 0 0 0 45 0 1 1 1
  inspect entity Box
  verify component Box Mesh
  save SampleProject/Assets/Scenes/Generated
`);
}

function printQueuedCommands(commands: string[]): void {
    if (commands.length === 0) {
        console.log("No queued authoring commands.");
        return;
    }

    console.log(commands.map((part) => JSON.stringify(part)).join(" "));
}

function appendQueuedCommand(commands: string[], tokens: string[]): void {
    commands.push(...tokens);
    console.log(`Queued: ${tokens.join(" ")}`);
}

async function promptRequired(rl: ReturnType<typeof createInterface>, label: string): Promise<string> {
    const value = (await rl.question(`${label}: `)).trim();
    if (value.length === 0) {
        throw new Error(`${label} cannot be empty.`);
    }
    return value;
}

async function promptVec3(rl: ReturnType<typeof createInterface>, label: string, fallback: Vec3): Promise<Vec3> {
    const value = (await rl.question(`${label} [${fallback.join(" ")}]: `)).trim();
    if (value.length === 0) {
        return fallback;
    }

    const parts = value.split(/\s+/);
    if (parts.length !== 3) {
        throw new Error(`${label} must contain exactly three numbers.`);
    }

    return [
        requireNumber(Number(parts[0]), `${label}.x`),
        requireNumber(Number(parts[1]), `${label}.y`),
        requireNumber(Number(parts[2]), `${label}.z`),
    ];
}

async function runInteractiveMenu(
    rl: ReturnType<typeof createInterface>,
    host: string,
    queuedCommands: string[],
    dryRun: boolean,
): Promise<boolean> {
    console.log(`Menu:
  1. New scene
  2. Add primitive
  3. Add point light
  4. Add camera
  5. Set transform
  6. Save scene
  7. Run queued commands
  8. Show queued commands
  9. Exit
`);
    const choice = (await rl.question("Choose: ")).trim();

    switch (choice) {
        case "1":
            appendQueuedCommand(queuedCommands, ["new"]);
            return true;
        case "2": {
            const alias = await promptRequired(rl, "Alias");
            const mesh = await promptRequired(rl, "Mesh (Cube/Sphere/Plane/Cylinder/Cone)");
            appendQueuedCommand(queuedCommands, ["primitive", alias, mesh]);
            return true;
        }
        case "3": {
            const alias = await promptRequired(rl, "Alias");
            appendQueuedCommand(queuedCommands, ["point-light", alias]);
            return true;
        }
        case "4": {
            const alias = await promptRequired(rl, "Alias");
            appendQueuedCommand(queuedCommands, ["camera", alias]);
            return true;
        }
        case "5": {
            const entity = await promptRequired(rl, "Entity alias or UUID");
            const translation = await promptVec3(rl, "Translation", [0, 0, 0]);
            const rotation = await promptVec3(rl, "Rotation degrees", [0, 0, 0]);
            const scale = await promptVec3(rl, "Scale", [1, 1, 1]);
            appendQueuedCommand(queuedCommands, [
                "transform",
                entity,
                ...translation.map(String),
                ...rotation.map(String),
                ...scale.map(String),
            ]);
            return true;
        }
        case "6": {
            const path = await promptRequired(rl, "Scene path");
            appendQueuedCommand(queuedCommands, ["save", path]);
            return true;
        }
        case "7":
            {
                const status = runHost(host, queuedCommands, dryRun);
                if (status !== 0) {
                    throw new Error(`Host exited with status ${status}.`);
                }
            }
            return true;
        case "8":
            printQueuedCommands(queuedCommands);
            return true;
        case "9":
            return false;
        default:
            console.log("Unknown menu choice.");
            return true;
    }
}

type InteractiveContext = {
    host: string;
    project?: string;
    jsonOutput: boolean;
    queuedCommands: string[];
    dryRun: boolean;
};

function baseHostArgs(project: string | undefined, jsonOutput: boolean): string[] {
    return [
        ...(jsonOutput ? ["--json"] : []),
        ...(project == null ? [] : ["--project", project]),
    ];
}

async function handleInteractiveLine(
    line: string,
    context: InteractiveContext,
    rl?: ReturnType<typeof createInterface>,
): Promise<number | null> {
    const tokens = splitCommandLine(line);
    const command = tokens[0];

    if (command === "exit" || command === "quit") {
        return 0;
    }
    if (command === "help") {
        printInteractiveHelp();
        return null;
    }
    if (command === "menu") {
        if (rl == null) {
            throw new Error("The menu command requires an interactive terminal.");
        }
        if (!(await runInteractiveMenu(rl, context.host, context.queuedCommands, context.dryRun))) {
            return 0;
        }
        return null;
    }
    if (command === "show") {
        printQueuedCommands(context.queuedCommands);
        return null;
    }
    if (command === "clear") {
        context.queuedCommands.splice(
            0,
            context.queuedCommands.length,
            ...baseHostArgs(context.project, context.jsonOutput),
        );
        console.log("Queued commands cleared.");
        return null;
    }
    if (command === "run" || command === "execute") {
        const status = runHost(context.host, context.queuedCommands, context.dryRun);
        return status === 0 ? null : status;
    }
    if (command === "preview") {
        const status = runHost(context.host, [...context.queuedCommands, "summary"], context.dryRun);
        return status === 0 ? null : status;
    }

    appendQueuedCommand(context.queuedCommands, tokens);
    return null;
}

async function runInteractive(
    host: string,
    project: string | undefined,
    jsonOutput: boolean,
    dryRun: boolean,
): Promise<number> {
    const context: InteractiveContext = {
        host,
        project,
        jsonOutput,
        queuedCommands: baseHostArgs(project, jsonOutput),
        dryRun,
    };

    if (!input.isTTY) {
        const lines = readFileSync(0, "utf8").split(/\r?\n/);
        for (const line of lines) {
            const trimmed = line.trim();
            if (trimmed.length === 0) {
                continue;
            }

            try {
                const status = await handleInteractiveLine(trimmed, context);
                if (status != null) {
                    return status;
                }
            } catch (error) {
                console.error(error instanceof Error ? error.message : String(error));
            }
        }
        return 0;
    }

    const rl = createInterface({ input, output });

    console.log("Luna interactive TS client. Type 'help' for commands.");
    try {
        while (true) {
            const line = (await rl.question("luna> ")).trim();
            if (line.length === 0) {
                continue;
            }

            try {
                const status = await handleInteractiveLine(line, context, rl);
                if (status != null) {
                    return status;
                }
            } catch (error) {
                console.error(error instanceof Error ? error.message : String(error));
            }
        }
    } finally {
        rl.close();
    }
}

async function main(): Promise<number> {
    const options = parseClientOptions(process.argv.slice(2));
    const command = options.passthrough[0];
    if (command == null || command === "--help" || command === "-h" || command === "help") {
        printUsage();
        return command == null ? 1 : 0;
    }

    const host = options.host == null ? findDefaultHost() : resolve(options.host);
    let hostArgs: string[];
    let project = options.project;

    if (command === "run") {
        hostArgs = options.passthrough.slice(1);
    } else if (command === "plan") {
        const planPath = options.passthrough[1];
        if (planPath == null) {
            throw new Error("Missing plan path.");
        }
        const plan = readPlan(planPath);
        project = project ?? plan.project;
        hostArgs = plan.commands.flatMap(planCommandToArgs);
    } else if (command === "interactive") {
        return await runInteractive(host, project, options.jsonOutput, options.dryRun);
    } else {
        throw new Error(`Unknown TS client command '${command}'. Use 'run', 'plan', or 'interactive'.`);
    }

    if (project != null) {
        hostArgs = ["--project", project, ...hostArgs];
    }
    if (options.jsonOutput) {
        hostArgs = ["--json", ...hostArgs];
    }

    return runHost(host, hostArgs, options.dryRun);
}

try {
    process.exitCode = await main();
} catch (error) {
    console.error(error instanceof Error ? error.message : String(error));
    process.exitCode = 1;
}
