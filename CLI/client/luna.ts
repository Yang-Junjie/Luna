#!/usr/bin/env node

import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, rmdirSync, unlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { createInterface } from "node:readline/promises";
import { stdin as input, stdout as output } from "node:process";
import { fileURLToPath } from "node:url";

import {
    AuthoringPlanBuilder,
    parseAuthoringCommandTokens,
    readAuthoringPlanJson,
    requireFiniteNumber,
    writeAuthoringPlanJson,
    type PlanCommand,
    type Vec3,
} from "./authoringProtocol.ts";

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
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts [--host <path>] [--project <path>] [--json] [--dry-run] run <commands...>
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts [--host <path>] [--project <path>] [--json] [--dry-run] plan <plan.json>
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts [--host <path>] [--json] [--dry-run] interactive

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

function runHost(host: string, args: string[], dryRun: boolean): number {
    const result = spawnSync(host, dryRun ? ["--dry-run", ...args] : args, { stdio: "inherit" });
    if (typeof result.status === "number") {
        return result.status;
    }
    if (result.error != null) {
        throw result.error;
    }
    return 1;
}

function runAuthoringPlan(
    host: string,
    commands: PlanCommand[],
    project: string | undefined,
    jsonOutput: boolean,
    dryRun: boolean,
): number {
    if (commands.length === 0) {
        throw new Error("No queued authoring commands.");
    }

    const tempDir = mkdtempSync(join(tmpdir(), "luna-authoring-"));
    const planPath = join(tempDir, "plan.json");
    writeFileSync(planPath, writeAuthoringPlanJson({ project, commands }), "utf8");

    try {
        const args = [
            ...(jsonOutput ? ["--json"] : []),
            "plan",
            planPath,
        ];
        return runHost(host, args, dryRun);
    } finally {
        try {
            unlinkSync(planPath);
            rmdirSync(tempDir);
        } catch {
            // Best-effort cleanup only.
        }
    }
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

Authoring commands are parsed into AuthoringPlan JSON before execution:
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

function printQueuedCommands(commands: PlanCommand[], project: string | undefined): void {
    if (commands.length === 0) {
        console.log("No queued authoring commands.");
        return;
    }

    console.log(writeAuthoringPlanJson({ project, commands }).trimEnd());
}

function appendQueuedCommand(commands: PlanCommand[], command: PlanCommand): void {
    commands.push(command);
    console.log(`Queued: ${JSON.stringify(command)}`);
}

function appendBuiltCommand(
    commands: PlanCommand[],
    build: (builder: AuthoringPlanBuilder) => AuthoringPlanBuilder,
): void {
    const [command] = build(new AuthoringPlanBuilder()).toPlan().commands;
    if (command == null) {
        throw new Error("No authoring command was built.");
    }
    appendQueuedCommand(commands, command);
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
        requireFiniteNumber(Number(parts[0]), `${label}.x`),
        requireFiniteNumber(Number(parts[1]), `${label}.y`),
        requireFiniteNumber(Number(parts[2]), `${label}.z`),
    ];
}

async function runInteractiveMenu(
    rl: ReturnType<typeof createInterface>,
    context: InteractiveContext,
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
            appendBuiltCommand(context.queuedCommands, (builder) => builder.newScene());
            return true;
        case "2": {
            const alias = await promptRequired(rl, "Alias");
            const mesh = await promptRequired(rl, "Mesh (Cube/Sphere/Plane/Cylinder/Cone)");
            appendBuiltCommand(context.queuedCommands, (builder) => builder.primitive(alias, mesh));
            return true;
        }
        case "3": {
            const alias = await promptRequired(rl, "Alias");
            appendBuiltCommand(context.queuedCommands, (builder) => builder.pointLight(alias));
            return true;
        }
        case "4": {
            const alias = await promptRequired(rl, "Alias");
            appendBuiltCommand(context.queuedCommands, (builder) => builder.camera(alias));
            return true;
        }
        case "5": {
            const entity = await promptRequired(rl, "Entity alias or UUID");
            const translation = await promptVec3(rl, "Translation", [0, 0, 0]);
            const rotation = await promptVec3(rl, "Rotation degrees", [0, 0, 0]);
            const scale = await promptVec3(rl, "Scale", [1, 1, 1]);
            appendBuiltCommand(
                context.queuedCommands,
                (builder) => builder.transform(entity, translation, rotation, scale),
            );
            return true;
        }
        case "6": {
            const path = await promptRequired(rl, "Scene path");
            appendBuiltCommand(context.queuedCommands, (builder) => builder.saveScene(path));
            return true;
        }
        case "7":
            {
                const status = runAuthoringPlan(
                    context.host,
                    context.queuedCommands,
                    context.project,
                    context.jsonOutput,
                    context.dryRun,
                );
                if (status !== 0) {
                    throw new Error(`Host exited with status ${status}.`);
                }
            }
            return true;
        case "8":
            printQueuedCommands(context.queuedCommands, context.project);
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
    queuedCommands: PlanCommand[];
    dryRun: boolean;
};

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
        if (!(await runInteractiveMenu(rl, context))) {
            return 0;
        }
        return null;
    }
    if (command === "show") {
        printQueuedCommands(context.queuedCommands, context.project);
        return null;
    }
    if (command === "clear") {
        context.queuedCommands.splice(0, context.queuedCommands.length);
        console.log("Queued commands cleared.");
        return null;
    }
    if (command === "run" || command === "execute") {
        const status = runAuthoringPlan(
            context.host,
            context.queuedCommands,
            context.project,
            context.jsonOutput,
            context.dryRun,
        );
        return status === 0 ? null : status;
    }
    if (command === "preview") {
        const status = runAuthoringPlan(
            context.host,
            [...context.queuedCommands, { op: "summary" }],
            context.project,
            context.jsonOutput,
            context.dryRun,
        );
        return status === 0 ? null : status;
    }

    appendQueuedCommand(context.queuedCommands, parseAuthoringCommandTokens(tokens));
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
        queuedCommands: [],
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
                return 1;
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
        const plan = readAuthoringPlanJson(planPath);
        project = project ?? plan.project;
        hostArgs = ["plan", planPath];
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
