#!/usr/bin/env node

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmdirSync, unlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { createInterface } from "node:readline/promises";
import { stdin as input, stdout as output } from "node:process";
import { fileURLToPath } from "node:url";

import { AuthoringHostClient } from "./authoringHostClient.ts";
import { AuthoringSessionController } from "./authoringController.ts";
import {
    AuthoringChatTranscript,
    renderAuthoringChatTurnSummary,
    renderAuthoringSessionState,
} from "./authoringChat.ts";
import {
    createAuthoringEvalRunDirectory,
    readAuthoringEvalSpec,
    renderAuthoringEvalSummary,
    runAuthoringEval,
    type AuthoringEvalResult,
    writeAuthoringEvalResult,
} from "./authoringEval.ts";
import { loadLunaEnvironment } from "./dotenv.ts";
import { createOpenAiCompatibleAuthoringPlanner } from "./authoringAiPlanner.ts";
import { OpenAiCompatibleClient } from "./openAiCompatibleProvider.ts";
import {
    AuthoringTurn,
    buildAuthoringTurnTransactionName,
    type AuthoringTurnPlanner,
    type AuthoringTurnResult,
} from "./authoringTurn.ts";
import {
    AuthoringPlanBuilder,
    parseAuthoringCommandTokens,
    readAuthoringPlanJson,
    requireFiniteNumber,
    writeAuthoringPlanJson,
    type AuthoringCapabilitiesDocument,
    type AuthoringPlan,
    type AuthoringRepairContext,
    type AuthoringReport,
    type PlanCommand,
    type Vec3,
} from "./authoringProtocol.ts";

type ClientOptions = {
    host?: string;
    authoringHost?: string;
    project?: string;
    planner: "local" | "ai";
    aiProvider?: string;
    aiBaseURL?: string;
    aiApiKey?: string;
    aiApiKeyEnv?: string;
    aiModel?: string;
    aiReasoningEffort?: string;
    aiThinking: boolean;
    aiTemperature?: number;
    aiTimeoutMs?: number;
    jsonOutput: boolean;
    dryRun: boolean;
    execute: boolean;
    passthrough: string[];
};

type ComposeResult = {
    ok: boolean;
    mode: "dry-run" | "execute";
    intent: string;
    plan: AuthoringPlan;
    report: AuthoringReport;
    repair: AuthoringRepairContext;
};

type TurnRequest = {
    intent: string;
    execute: boolean;
    maxAttempts: number;
    printPlanOnly: boolean;
    planner?: "local" | "ai";
};

type ChatRequest = {
    execute: boolean;
    maxAttempts: number;
    planner?: "local" | "ai";
};

type EvalRequest = {
    specPath: string;
    execute?: boolean;
    maxAttempts?: number;
    planner?: "local" | "ai";
    out?: string;
    outDir?: string;
};

type EvalSuiteRequest = {
    specPaths: string[];
    execute?: boolean;
    maxAttempts?: number;
    planner?: "local" | "ai";
    out?: string;
    outDir?: string;
};

type EvalSuiteResult = {
    protocol: {
        name: "luna.authoring.eval-suite";
        version: 1;
    };
    ok: boolean;
    name: string;
    startedAt: string;
    completedAt: string;
    runDir: string;
    resultPath?: string;
    mode: "execute" | "rollback";
    evals: EvalSuiteEntry[];
};

type EvalSuiteEntry = {
    name: string;
    specPath: string;
    ok: boolean;
    resultPath?: string;
    turnCount: number;
    failedTurns: Array<{
        id: string;
        failures: string[];
    }>;
};

type TurnResult = AuthoringTurnResult & {
    mode: "dry-run" | "execute";
    plan: AuthoringPlan;
};

function printUsage(): void {
    console.log(`Luna TS CLI

Usage:
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts [--host <path>] [--project <path>] [--json] [--dry-run] run <commands...>
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts [--host <path>] [--project <path>] [--json] [--dry-run] plan <plan.json>
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts [--authoring-host <path>] capabilities
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts [--authoring-host <path>] [--project <path>] [--execute] compose "<intent>"
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts [--authoring-host <path>] [--project <path>] [--execute] [--ai] [--ai-provider deepseek] [--ai-timeout-ms <ms>] turn "<intent>" [--max-attempts <n>]
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts [--authoring-host <path>] [--project <path>] [--ai] [--ai-timeout-ms <ms>] chat|session
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts [--authoring-host <path>] [--project <path>] [--ai] eval <eval.json> [--out-dir <dir>] [--max-attempts <n>]
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts [--authoring-host <path>] [--project <path>] [--ai] eval-suite [eval.json ...] [--out-dir <dir>] [--max-attempts <n>]
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts [--authoring-host <path>] [--json] [--dry-run] interactive

Examples:
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts run new primitive CubeBox Cube save build/CLI/Smoke/TsScene
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts plan CLI/client/examples/smoke.plan.json
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts compose "create a simple scene with a cube, camera, and light"
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts turn "create a simple scene with a cube, camera, and light"
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts --ai --ai-provider deepseek --ai-timeout-ms 300000 turn "create a simple scene with a cube"
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts --ai chat
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts --ai session
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts --ai eval CLI/tests/ai-evals/basic-scene.json
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts --ai eval-suite
  node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts interactive

The TypeScript client uses LunaCLI for legacy command execution and LunaAuthoringHost through a session controller for stateful authoring flows.`);
}

function takeOptionValue(args: string[], index: number, option: string): string {
    const value = args[index + 1];
    if (value == null || value.startsWith("--")) {
        throw new Error(`Missing value for ${option}`);
    }
    return value;
}

function parseClientOptions(args: string[]): ClientOptions {
    const options: ClientOptions = {
        planner: "local",
        aiThinking: false,
        jsonOutput: false,
        dryRun: false,
        execute: false,
        passthrough: [],
    };

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
        if (arg === "--authoring-host") {
            options.authoringHost = takeOptionValue(args, index, arg);
            index += 2;
            continue;
        }
        if (arg.startsWith("--authoring-host=")) {
            options.authoringHost = arg.slice("--authoring-host=".length);
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
        if (arg === "--planner") {
            options.planner = parsePlannerName(takeOptionValue(args, index, arg));
            index += 2;
            continue;
        }
        if (arg.startsWith("--planner=")) {
            options.planner = parsePlannerName(arg.slice("--planner=".length));
            index += 1;
            continue;
        }
        if (arg === "--ai") {
            options.planner = "ai";
            index += 1;
            continue;
        }
        if (arg === "--ai-provider") {
            options.aiProvider = takeOptionValue(args, index, arg);
            index += 2;
            continue;
        }
        if (arg.startsWith("--ai-provider=")) {
            options.aiProvider = arg.slice("--ai-provider=".length);
            index += 1;
            continue;
        }
        if (arg === "--ai-base-url") {
            options.aiBaseURL = takeOptionValue(args, index, arg);
            index += 2;
            continue;
        }
        if (arg.startsWith("--ai-base-url=")) {
            options.aiBaseURL = arg.slice("--ai-base-url=".length);
            index += 1;
            continue;
        }
        if (arg === "--ai-api-key") {
            options.aiApiKey = takeOptionValue(args, index, arg);
            index += 2;
            continue;
        }
        if (arg.startsWith("--ai-api-key=")) {
            options.aiApiKey = arg.slice("--ai-api-key=".length);
            index += 1;
            continue;
        }
        if (arg === "--ai-api-key-env") {
            options.aiApiKeyEnv = takeOptionValue(args, index, arg);
            index += 2;
            continue;
        }
        if (arg.startsWith("--ai-api-key-env=")) {
            options.aiApiKeyEnv = arg.slice("--ai-api-key-env=".length);
            index += 1;
            continue;
        }
        if (arg === "--ai-model") {
            options.aiModel = takeOptionValue(args, index, arg);
            index += 2;
            continue;
        }
        if (arg.startsWith("--ai-model=")) {
            options.aiModel = arg.slice("--ai-model=".length);
            index += 1;
            continue;
        }
        if (arg === "--ai-reasoning-effort") {
            options.aiReasoningEffort = takeOptionValue(args, index, arg);
            index += 2;
            continue;
        }
        if (arg.startsWith("--ai-reasoning-effort=")) {
            options.aiReasoningEffort = arg.slice("--ai-reasoning-effort=".length);
            index += 1;
            continue;
        }
        if (arg === "--ai-thinking") {
            options.aiThinking = true;
            index += 1;
            continue;
        }
        if (arg === "--ai-temperature") {
            options.aiTemperature = parseFiniteOption(takeOptionValue(args, index, arg), arg);
            index += 2;
            continue;
        }
        if (arg.startsWith("--ai-temperature=")) {
            options.aiTemperature = parseFiniteOption(arg.slice("--ai-temperature=".length), "--ai-temperature");
            index += 1;
            continue;
        }
        if (arg === "--ai-timeout-ms") {
            options.aiTimeoutMs = parsePositiveIntegerOption(takeOptionValue(args, index, arg), arg);
            index += 2;
            continue;
        }
        if (arg.startsWith("--ai-timeout-ms=")) {
            options.aiTimeoutMs = parsePositiveIntegerOption(arg.slice("--ai-timeout-ms=".length), "--ai-timeout-ms");
            index += 1;
            continue;
        }
        if (arg === "--dry-run") {
            options.dryRun = true;
            index += 1;
            continue;
        }
        if (arg === "--execute") {
            options.execute = true;
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

function parsePlannerName(value: string): "local" | "ai" {
    if (value === "local" || value === "ai") {
        return value;
    }
    throw new Error("Planner must be 'local' or 'ai'.");
}

function parseFiniteOption(value: string, option: string): number {
    return requireFiniteNumber(Number(value), option);
}

function parsePositiveIntegerOption(value: string, option: string): number {
    const parsed = requireFiniteNumber(Number(value), option);
    if (!Number.isInteger(parsed) || parsed < 1) {
        throw new Error(`Option '${option}' must be a positive integer.`);
    }
    return parsed;
}

function findDefaultLegacyHost(): string {
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

function findDefaultAuthoringHost(): string {
    const scriptDir = dirname(fileURLToPath(import.meta.url));
    const repoRoot = resolve(scriptDir, "..", "..");
    const candidates = [
        resolve(repoRoot, "build", "CLI", "LunaAuthoringHost.exe"),
        resolve(repoRoot, "build", "CLI", "LunaAuthoringHost"),
        resolve(process.cwd(), "build", "CLI", "LunaAuthoringHost.exe"),
        resolve(process.cwd(), "build", "CLI", "LunaAuthoringHost"),
    ];

    for (const candidate of candidates) {
        if (existsSync(candidate)) {
            return candidate;
        }
    }

    throw new Error("Could not find LunaAuthoringHost. Build it first or pass --authoring-host <path>.");
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

async function withAuthoringHostClient<T>(
    host: string,
    fn: (client: AuthoringHostClient) => Promise<T>,
): Promise<T> {
    const client = new AuthoringHostClient(host);
    try {
        return await fn(client);
    } finally {
        await client.close();
    }
}

function buildComposeTransactionName(intent: string): string {
    const normalized = intent.trim().replace(/\s+/g, " ");
    return normalized.length === 0 ? "Compose Session" : `Compose: ${normalized.slice(0, 64)}`;
}

async function readCapabilitiesFromAuthoringHost(host: string): Promise<AuthoringCapabilitiesDocument> {
    return await withAuthoringHostClient(host, async (client) => client.capabilities());
}

function printCapabilities(capabilities: AuthoringCapabilitiesDocument): void {
    console.log(JSON.stringify(capabilities, null, 2));
}

function createOpenAiCompatibleClient(options: ClientOptions): OpenAiCompatibleClient {
    const provider = (options.aiProvider ?? process.env.LUNA_AI_PROVIDER ?? "deepseek").toLowerCase();
    const defaultBaseURL = provider === "deepseek"
        ? "https://api.deepseek.com"
        : provider === "openai"
            ? "https://api.openai.com/v1"
            : undefined;
    const defaultApiKeyEnv = provider === "deepseek"
        ? "DEEPSEEK_API_KEY"
        : provider === "openai"
            ? "OPENAI_API_KEY"
            : "LUNA_AI_API_KEY";
    const defaultModel = provider === "deepseek" ? "deepseek-v4-pro" : undefined;

    const baseURL = firstNonEmpty(options.aiBaseURL, process.env.LUNA_AI_BASE_URL, defaultBaseURL);
    if (baseURL == null) {
        throw new Error("AI base URL is required. Pass --ai-base-url or set LUNA_AI_BASE_URL.");
    }

    const apiKeyEnv = firstNonEmpty(options.aiApiKeyEnv, process.env.LUNA_AI_API_KEY_ENV, defaultApiKeyEnv);
    const apiKey = firstNonEmpty(options.aiApiKey, process.env.LUNA_AI_API_KEY, apiKeyEnv == null ? undefined : process.env[apiKeyEnv]);
    if (apiKey == null) {
        throw new Error(`AI API key is required. Pass --ai-api-key or set ${apiKeyEnv ?? "LUNA_AI_API_KEY"}.`);
    }

    const model = firstNonEmpty(
        options.aiModel,
        process.env.LUNA_AI_MODEL,
        provider === "openai" ? process.env.OPENAI_MODEL : undefined,
        defaultModel,
    );
    if (model == null) {
        throw new Error("AI model is required. Pass --ai-model or set LUNA_AI_MODEL.");
    }

    const timeoutMs = options.aiTimeoutMs ??
        (process.env.LUNA_AI_TIMEOUT_MS == null
            ? undefined
            : parsePositiveIntegerOption(process.env.LUNA_AI_TIMEOUT_MS, "LUNA_AI_TIMEOUT_MS"));
    const extraRequestFields: Record<string, unknown> = {};
    const reasoningEffort = firstNonEmpty(options.aiReasoningEffort, process.env.LUNA_AI_REASONING_EFFORT);
    if (reasoningEffort != null) {
        extraRequestFields.reasoning_effort = reasoningEffort;
    }
    if (options.aiThinking || parseEnvBoolean(process.env.LUNA_AI_THINKING)) {
        extraRequestFields.thinking = { type: "enabled" };
    }

    return new OpenAiCompatibleClient({
        baseURL,
        apiKey,
        model,
        timeoutMs,
        extraRequestFields,
    });
}

function firstNonEmpty(...values: Array<string | undefined>): string | undefined {
    return values.find((value) => value != null && value.trim().length > 0);
}

function parseEnvBoolean(value: string | undefined): boolean {
    if (value == null) {
        return false;
    }
    const normalized = value.trim().toLowerCase();
    return normalized === "1" || normalized === "true" || normalized === "yes" || normalized === "enabled";
}

function requireCapability(capabilities: AuthoringCapabilitiesDocument, op: string): void {
    if (!capabilities.capabilities.some((capability) => capability.op === op)) {
        throw new Error(`Host does not expose required authoring capability '${op}'.`);
    }
}

function containsAny(text: string, words: string[]): boolean {
    return words.some((word) => text.includes(word));
}

function composeLocalPlan(intent: string, capabilities: AuthoringCapabilitiesDocument, project?: string): AuthoringPlan {
    const lower = intent.toLowerCase();
    const builder = new AuthoringPlanBuilder();
    if (project != null) {
        builder.project(project);
    }

    requireCapability(capabilities, "new");
    builder.newScene();

    const wants_floor = containsAny(lower, ["floor", "ground", "plane", "地面", "平面"]);
    const wants_sphere = containsAny(lower, ["sphere", "ball", "orb", "球"]);
    const wants_cylinder = containsAny(lower, ["cylinder", "柱"]);
    const wants_cone = containsAny(lower, ["cone", "锥"]);
    const wants_cube = containsAny(lower, ["cube", "box", "block", "simple", "简单", "方块", "立方体"]) ||
                       (!wants_floor && !wants_sphere && !wants_cylinder && !wants_cone);

    if (wants_floor) {
        requireCapability(capabilities, "primitive");
        requireCapability(capabilities, "transform");
        builder.primitive("Floor", "Plane")
            .transform("Floor", [0, -0.5, 0], [0, 0, 0], [8, 1, 8]);
    }

    if (wants_cube) {
        requireCapability(capabilities, "primitive");
        requireCapability(capabilities, "transform");
        builder.primitive("Cube", "Cube")
            .transform("Cube", [0, 0, 0], [0, 35, 0], [1, 1, 1]);
    }
    if (wants_sphere) {
        requireCapability(capabilities, "primitive");
        requireCapability(capabilities, "transform");
        builder.primitive("Sphere", "Sphere")
            .transform("Sphere", [0, 0, 0], [0, 0, 0], [1, 1, 1]);
    }
    if (wants_cylinder) {
        requireCapability(capabilities, "primitive");
        requireCapability(capabilities, "transform");
        builder.primitive("Cylinder", "Cylinder")
            .transform("Cylinder", [0, 0, 0], [0, 0, 0], [1, 1, 1]);
    }
    if (wants_cone) {
        requireCapability(capabilities, "primitive");
        requireCapability(capabilities, "transform");
        builder.primitive("Cone", "Cone")
            .transform("Cone", [0, 0, 0], [0, 0, 0], [1, 1, 1]);
    }

    requireCapability(capabilities, "camera");
    requireCapability(capabilities, "transform");
    requireCapability(capabilities, "camera-perspective");
    builder.camera("MainCamera")
        .transform("MainCamera", [0, 3, 7], [-20, 0, 0], [1, 1, 1])
        .cameraPerspective("MainCamera", 60, 0.1, 500);

    requireCapability(capabilities, "point-light");
    requireCapability(capabilities, "transform");
    requireCapability(capabilities, "light-intensity");
    requireCapability(capabilities, "light-color");
    builder.pointLight("KeyLight")
        .transform("KeyLight", [2, 4, 2], [0, 0, 0], [1, 1, 1])
        .lightIntensity("KeyLight", 3.5)
        .lightColor("KeyLight", [1, 0.92, 0.78]);

    requireCapability(capabilities, "inspect");
    requireCapability(capabilities, "verify");
    requireCapability(capabilities, "snapshot");
    builder.inspectHierarchy()
        .verifyEntityCountAtLeast(1)
        .summary()
        .snapshot();

    return builder.toPlan();
}

function printPlan(plan: AuthoringPlan): void {
    console.log(writeAuthoringPlanJson(plan).trimEnd());
}

async function runComposeWithAuthoringController(
    controller: AuthoringSessionController,
    intent: string,
    plan: AuthoringPlan,
    execute: boolean,
): Promise<ComposeResult> {
    const begin = await controller.beginTransaction(buildComposeTransactionName(intent));
    if (!begin.ok) {
        throw new Error(`Could not open authoring transaction for compose intent '${intent}'.`);
    }

    try {
        const result = await controller.executePlan(plan);
        if (execute) {
            if (result.ok) {
                await controller.commitTransaction();
            } else {
                await controller.rollbackTransaction();
            }
        } else {
            await controller.rollbackTransaction();
        }

        return {
            ok: result.ok,
            mode: execute ? "execute" : "dry-run",
            intent,
            plan,
            report: result.report,
            repair: result.repair,
        };
    } catch (error) {
        try {
            await controller.rollbackTransaction();
        } catch {
            // Best-effort cleanup only.
        }
        throw error;
    }
}

function parseTurnRequest(args: string[], globalExecute: boolean): TurnRequest {
    let execute = globalExecute;
    let printPlanOnly = false;
    let maxAttempts = 1;
    let planner: "local" | "ai" | undefined;
    const intentParts: string[] = [];

    for (let index = 0; index < args.length; ++index) {
        const arg = args[index];
        if (arg === "--ai") {
            planner = "ai";
            continue;
        }
        if (arg === "--planner") {
            const value = args[index + 1];
            if (value == null || value.startsWith("--")) {
                throw new Error("Missing value for --planner.");
            }
            planner = parsePlannerName(value);
            ++index;
            continue;
        }
        if (arg.startsWith("--planner=")) {
            planner = parsePlannerName(arg.slice("--planner=".length));
            continue;
        }
        if (arg === "--execute") {
            execute = true;
            continue;
        }
        if (arg === "--dry-run") {
            execute = false;
            continue;
        }
        if (arg === "--plan-only") {
            printPlanOnly = true;
            continue;
        }
        if (arg === "--max-attempts") {
            const value = args[index + 1];
            if (value == null || value.startsWith("--")) {
                throw new Error("Missing value for --max-attempts.");
            }
            maxAttempts = parsePositiveIntegerOption(value, "--max-attempts");
            ++index;
            continue;
        }
        if (arg.startsWith("--max-attempts=")) {
            maxAttempts = parsePositiveIntegerOption(arg.slice("--max-attempts=".length), "--max-attempts");
            continue;
        }
        intentParts.push(arg);
    }

    const intent = intentParts.join(" ").trim();
    if (intent.length === 0) {
        throw new Error("Missing turn intent.");
    }

    return { intent, execute, maxAttempts, printPlanOnly, planner };
}

function parseChatRequest(args: string[]): ChatRequest {
    let execute = true;
    let maxAttempts = 1;
    let planner: "local" | "ai" | undefined;

    for (let index = 0; index < args.length; ++index) {
        const arg = args[index];
        if (arg === "--ai") {
            planner = "ai";
            continue;
        }
        if (arg === "--planner") {
            const value = args[index + 1];
            if (value == null || value.startsWith("--")) {
                throw new Error("Missing value for --planner.");
            }
            planner = parsePlannerName(value);
            ++index;
            continue;
        }
        if (arg.startsWith("--planner=")) {
            planner = parsePlannerName(arg.slice("--planner=".length));
            continue;
        }
        if (arg === "--execute") {
            execute = true;
            continue;
        }
        if (arg === "--dry-run") {
            execute = false;
            continue;
        }
        if (arg === "--max-attempts") {
            const value = args[index + 1];
            if (value == null || value.startsWith("--")) {
                throw new Error("Missing value for --max-attempts.");
            }
            maxAttempts = parsePositiveIntegerOption(value, "--max-attempts");
            ++index;
            continue;
        }
        if (arg.startsWith("--max-attempts=")) {
            maxAttempts = parsePositiveIntegerOption(arg.slice("--max-attempts=".length), "--max-attempts");
            continue;
        }

        throw new Error(`Command 'chat' does not accept positional argument '${arg}'.`);
    }

    return { execute, maxAttempts, planner };
}

function parseEvalRequest(args: string[], options: ClientOptions): EvalRequest {
    let execute = options.execute ? true : options.dryRun ? false : undefined;
    let maxAttempts: number | undefined;
    let planner: "local" | "ai" | undefined;
    let out: string | undefined;
    let outDir: string | undefined;
    let specPath: string | undefined;

    for (let index = 0; index < args.length; ++index) {
        const arg = args[index];
        if (arg === "--ai") {
            planner = "ai";
            continue;
        }
        if (arg === "--planner") {
            const value = args[index + 1];
            if (value == null || value.startsWith("--")) {
                throw new Error("Missing value for --planner.");
            }
            planner = parsePlannerName(value);
            ++index;
            continue;
        }
        if (arg.startsWith("--planner=")) {
            planner = parsePlannerName(arg.slice("--planner=".length));
            continue;
        }
        if (arg === "--execute") {
            execute = true;
            continue;
        }
        if (arg === "--dry-run") {
            execute = false;
            continue;
        }
        if (arg === "--max-attempts") {
            const value = args[index + 1];
            if (value == null || value.startsWith("--")) {
                throw new Error("Missing value for --max-attempts.");
            }
            maxAttempts = parsePositiveIntegerOption(value, "--max-attempts");
            ++index;
            continue;
        }
        if (arg.startsWith("--max-attempts=")) {
            maxAttempts = parsePositiveIntegerOption(arg.slice("--max-attempts=".length), "--max-attempts");
            continue;
        }
        if (arg === "--out") {
            out = takeOptionValue(args, index, arg);
            ++index;
            continue;
        }
        if (arg.startsWith("--out=")) {
            out = arg.slice("--out=".length);
            continue;
        }
        if (arg === "--out-dir") {
            outDir = takeOptionValue(args, index, arg);
            ++index;
            continue;
        }
        if (arg.startsWith("--out-dir=")) {
            outDir = arg.slice("--out-dir=".length);
            continue;
        }
        if (arg.startsWith("--")) {
            throw new Error(`Unknown eval option '${arg}'.`);
        }
        if (specPath != null) {
            throw new Error(`Command 'eval' only accepts one eval spec path.`);
        }
        specPath = arg;
    }

    if (specPath == null) {
        throw new Error("Missing eval spec path.");
    }

    return { specPath, execute, maxAttempts, planner, out, outDir };
}

function parseEvalSuiteRequest(args: string[], options: ClientOptions): EvalSuiteRequest {
    let execute = options.execute ? true : options.dryRun ? false : undefined;
    let maxAttempts: number | undefined;
    let planner: "local" | "ai" | undefined;
    let out: string | undefined;
    let outDir: string | undefined;
    const specPaths: string[] = [];

    for (let index = 0; index < args.length; ++index) {
        const arg = args[index];
        if (arg === "--ai") {
            planner = "ai";
            continue;
        }
        if (arg === "--planner") {
            const value = args[index + 1];
            if (value == null || value.startsWith("--")) {
                throw new Error("Missing value for --planner.");
            }
            planner = parsePlannerName(value);
            ++index;
            continue;
        }
        if (arg.startsWith("--planner=")) {
            planner = parsePlannerName(arg.slice("--planner=".length));
            continue;
        }
        if (arg === "--execute") {
            execute = true;
            continue;
        }
        if (arg === "--dry-run") {
            execute = false;
            continue;
        }
        if (arg === "--max-attempts") {
            const value = args[index + 1];
            if (value == null || value.startsWith("--")) {
                throw new Error("Missing value for --max-attempts.");
            }
            maxAttempts = parsePositiveIntegerOption(value, "--max-attempts");
            ++index;
            continue;
        }
        if (arg.startsWith("--max-attempts=")) {
            maxAttempts = parsePositiveIntegerOption(arg.slice("--max-attempts=".length), "--max-attempts");
            continue;
        }
        if (arg === "--out") {
            out = takeOptionValue(args, index, arg);
            ++index;
            continue;
        }
        if (arg.startsWith("--out=")) {
            out = arg.slice("--out=".length);
            continue;
        }
        if (arg === "--out-dir") {
            outDir = takeOptionValue(args, index, arg);
            ++index;
            continue;
        }
        if (arg.startsWith("--out-dir=")) {
            outDir = arg.slice("--out-dir=".length);
            continue;
        }
        if (arg.startsWith("--")) {
            throw new Error(`Unknown eval-suite option '${arg}'.`);
        }
        specPaths.push(arg);
    }

    return { specPaths, execute, maxAttempts, planner, out, outDir };
}

function createLocalTurnPlanner(intent: string, project?: string): AuthoringTurnPlanner {
    return async ({ capabilities }) => composeLocalPlan(intent, capabilities, project);
}

function createTurnPlanner(intent: string, project: string | undefined, request: TurnRequest, options: ClientOptions): AuthoringTurnPlanner {
    const planner = request.planner ?? options.planner;
    if (planner === "ai") {
        return createOpenAiCompatibleAuthoringPlanner({
            client: createOpenAiCompatibleClient(options),
            temperature: options.aiTemperature ??
                (process.env.LUNA_AI_TEMPERATURE == null
                    ? undefined
                    : parseFiniteOption(process.env.LUNA_AI_TEMPERATURE, "LUNA_AI_TEMPERATURE")),
        });
    }

    return createLocalTurnPlanner(intent, project);
}

function finalTurnPlan(result: AuthoringTurnResult): AuthoringPlan {
    const attempt = result.attempts.at(-1);
    if (attempt == null) {
        throw new Error("Authoring turn did not produce a plan.");
    }
    return attempt.plan;
}

async function runTurnWithAuthoringController(
    controller: AuthoringSessionController,
    intent: string,
    request: TurnRequest,
    options: ClientOptions,
    project?: string,
): Promise<TurnResult> {
    const planner = createTurnPlanner(intent, project, request, options);
    const transactionName = buildAuthoringTurnTransactionName(intent);

    if (request.printPlanOnly) {
        const state = await controller.state();
        const plan = await planner({
            intent,
            project,
            attempt: 1,
            maxAttempts: request.maxAttempts,
            capabilities: state.capabilities,
            session: state.session,
        });
        printPlan(plan);
        return {
            ok: true,
            committed: false,
            mode: request.execute ? "execute" : "dry-run",
            intent,
            project,
            transactionName,
            maxAttempts: request.maxAttempts,
            capabilities: state.capabilities,
            sessionBefore: state.session,
            sessionAfter: state.session,
            attempts: [],
            report: {
                protocol: state.capabilities.protocol,
                ok: true,
                scene: state.session.scene,
                entities: [],
                savedScenes: [],
                inspections: [],
                verifications: [],
                diagnostics: [],
                errors: [],
            },
            repair: {
                ok: true,
                diagnostics: [],
                errors: [],
                retryable: false,
            },
            events: [],
            plan,
        };
    }

    const turn = new AuthoringTurn(controller, { transactionName });
    const result = await turn.run(intent, planner, {
        project,
        transactionName,
        maxAttempts: request.maxAttempts,
        commitOnSuccess: request.execute,
    });

    return {
        ...result,
        mode: request.execute ? "execute" : "dry-run",
        plan: finalTurnPlan(result),
    };
}

async function runEvalWithAuthoringController(
    controller: AuthoringSessionController,
    request: EvalRequest,
    options: ClientOptions,
    project?: string,
): Promise<number> {
    const result = await executeEvalWithAuthoringController(controller, request, options, project);
    if (options.jsonOutput) {
        console.log(JSON.stringify(result, null, 2));
    } else {
        console.log(renderAuthoringEvalSummary(result).trimEnd());
    }
    return result.ok ? 0 : 1;
}

async function executeEvalWithAuthoringController(
    controller: AuthoringSessionController,
    request: EvalRequest,
    options: ClientOptions,
    project?: string,
): Promise<AuthoringEvalResult> {
    const spec = readAuthoringEvalSpec(resolve(request.specPath));
    const outputRoot = resolve(request.outDir ?? join("logs", "ai-evals"));
    const runDir = createAuthoringEvalRunDirectory(outputRoot, spec.name);
    const resultPath = resolve(request.out ?? join(runDir, "result.json"));
    const result = await runAuthoringEval({
        controller,
        spec,
        project,
        runDir,
        resultPath,
        execute: request.execute,
        maxAttempts: request.maxAttempts,
        createPlanner: (intent) => createTurnPlanner(
            intent,
            project,
            {
                intent,
                execute: request.execute ?? spec.execute ?? true,
                maxAttempts: request.maxAttempts ?? spec.maxAttempts ?? 1,
                printPlanOnly: false,
                planner: request.planner,
            },
            options,
        ),
    });

    writeAuthoringEvalResult(resultPath, result);
    return result;
}

async function runEvalSuite(
    authoringHost: string,
    request: EvalSuiteRequest,
    options: ClientOptions,
    project?: string,
): Promise<number> {
    const result = await executeEvalSuite(authoringHost, request, options, project);
    if (options.jsonOutput) {
        console.log(JSON.stringify(result, null, 2));
    } else {
        console.log(renderEvalSuiteSummary(result).trimEnd());
    }
    return result.ok ? 0 : 1;
}

async function executeEvalSuite(
    authoringHost: string,
    request: EvalSuiteRequest,
    options: ClientOptions,
    project?: string,
): Promise<EvalSuiteResult> {
    const startedAt = new Date().toISOString();
    const specPaths = resolveEvalSuiteSpecPaths(request.specPaths);
    const outputRoot = resolve(request.outDir ?? join("logs", "ai-evals", "suites"));
    const runDir = createAuthoringEvalRunDirectory(outputRoot, "suite");
    const resultPath = resolve(request.out ?? join(runDir, "suite.json"));
    const entries: EvalSuiteEntry[] = [];

    for (const specPath of specPaths) {
        const evalRequest: EvalRequest = {
            specPath,
            execute: request.execute,
            maxAttempts: request.maxAttempts,
            planner: request.planner,
            outDir: runDir,
        };
        const result = await withAuthoringHostClient(authoringHost, async (client) => {
            const controller = new AuthoringSessionController(client);
            return await executeEvalWithAuthoringController(controller, evalRequest, options, project);
        });
        entries.push(toEvalSuiteEntry(specPath, result));
    }

    const completedAt = new Date().toISOString();
    const result: EvalSuiteResult = {
        protocol: { name: "luna.authoring.eval-suite", version: 1 },
        ok: entries.every((entry) => entry.ok),
        name: "authoring-eval-suite",
        startedAt,
        completedAt,
        runDir,
        resultPath,
        mode: (request.execute ?? true) ? "execute" : "rollback",
        evals: entries,
    };
    mkdirSync(dirname(resultPath), { recursive: true });
    writeFileSync(resultPath, `${JSON.stringify(result, null, 2)}\n`, "utf8");
    return result;
}

function resolveEvalSuiteSpecPaths(paths: string[]): string[] {
    if (paths.length > 0) {
        return paths.map((path) => resolve(path));
    }

    const scriptDir = dirname(fileURLToPath(import.meta.url));
    const repoRoot = resolve(scriptDir, "..", "..");
    const evalDir = resolve(repoRoot, "CLI", "tests", "ai-evals");
    const specs = readdirSync(evalDir)
        .filter((name) => name.endsWith(".json"))
        .sort((a, b) => a.localeCompare(b))
        .map((name) => resolve(evalDir, name));
    if (specs.length === 0) {
        throw new Error(`No eval specs found in ${evalDir}.`);
    }
    return specs;
}

function toEvalSuiteEntry(specPath: string, result: AuthoringEvalResult): EvalSuiteEntry {
    return {
        name: result.name,
        specPath,
        ok: result.ok,
        resultPath: result.resultPath,
        turnCount: result.turns.length,
        failedTurns: result.turns
            .filter((turn) => !turn.ok)
            .map((turn) => ({
                id: turn.id,
                failures: [...turn.failures],
            })),
    };
}

function renderEvalSuiteSummary(result: EvalSuiteResult): string {
    const passed = result.evals.filter((entry) => entry.ok).length;
    const lines = [
        `Authoring eval suite: ${result.ok ? "PASS" : "FAIL"}`,
        `Mode: ${result.mode}`,
        `Passed: ${passed}/${result.evals.length}`,
        `Run: ${result.runDir}`,
    ];
    if (result.resultPath != null) {
        lines.push(`Result: ${result.resultPath}`);
    }

    for (const entry of result.evals) {
        lines.push(`- ${entry.name}: ${entry.ok ? "PASS" : "FAIL"} (${entry.turnCount} turns)`);
        if (entry.resultPath != null) {
            lines.push(`  result: ${entry.resultPath}`);
        }
        for (const failedTurn of entry.failedTurns) {
            lines.push(`  ${failedTurn.id}: ${failedTurn.failures.join("; ")}`);
        }
    }
    return `${lines.join("\n")}\n`;
}

type ComposeRequest = {
    intent: string;
    execute: boolean;
    printPlanOnly: boolean;
};

function parseComposeRequest(args: string[], globalExecute: boolean): ComposeRequest {
    let execute = globalExecute;
    let printPlanOnly = false;
    const intentParts: string[] = [];

    for (const arg of args) {
        if (arg === "--execute") {
            execute = true;
            continue;
        }
        if (arg === "--dry-run") {
            execute = false;
            continue;
        }
        if (arg === "--plan-only") {
            printPlanOnly = true;
            continue;
        }
        intentParts.push(arg);
    }

    const intent = intentParts.join(" ").trim();
    if (intent.length === 0) {
        throw new Error("Missing compose intent.");
    }

    return { intent, execute, printPlanOnly };
}

function runLegacyAuthoringPlan(
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

function printReportSummary(report: AuthoringReport): void {
    console.log(`Scene: ${report.scene.name}`);
    console.log(`Scene File: ${report.scene.path ?? "<unsaved>"}`);
    console.log(`Entities: ${report.scene.entityCount}`);
    console.log(`Dirty: ${report.scene.dirty ? "true" : "false"}`);
}

function printExecutionResult(report: AuthoringReport, jsonOutput: boolean, dryRun: boolean, ok: boolean): void {
    if (jsonOutput) {
        console.log(JSON.stringify(report, null, 2));
        return;
    }

    printWarnings(report);
    if (ok) {
        if (dryRun) {
            console.log("Dry run: valid");
        } else {
            printEntityBindings(report);
            printReportSummary(report);
        }
        return;
    }

    for (const error of report.errors) {
        console.error(error);
    }
}

async function executeQueuedPlan(
    context: InteractiveContext,
    commands: PlanCommand[],
): Promise<number> {
    if (commands.length === 0) {
        throw new Error("No queued authoring commands.");
    }

    if (context.authoringClient != null && !context.dryRun) {
        const result = await context.authoringClient.executePlan({ project: context.project, commands });
        printExecutionResult(result.report, context.jsonOutput, false, result.ok);
        return result.ok ? 0 : 1;
    }

    if (context.legacyHost == null) {
        throw new Error("Legacy authoring host is required for dry-run interactive execution.");
    }

    return runLegacyAuthoringPlan(
        context.legacyHost,
        commands,
        context.project,
        context.jsonOutput,
        context.dryRun,
    );
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
  snapshot
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
                const status = await executeQueuedPlan(context, context.queuedCommands);
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
    legacyHost?: string;
    authoringClient?: AuthoringHostClient;
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
        const status = await executeQueuedPlan(context, context.queuedCommands);
        return status === 0 ? null : status;
    }
    if (command === "preview") {
        const status = await executeQueuedPlan(context, [...context.queuedCommands, { op: "summary" }]);
        return status === 0 ? null : status;
    }

    appendQueuedCommand(context.queuedCommands, parseAuthoringCommandTokens(tokens));
    return null;
}

async function runInteractive(
    legacyHost: string | undefined,
    authoringHost: string,
    project: string | undefined,
    jsonOutput: boolean,
    dryRun: boolean,
): Promise<number> {
    const authoringClient = dryRun ? undefined : new AuthoringHostClient(authoringHost);
    const context: InteractiveContext = {
        legacyHost,
        authoringClient,
        project,
        jsonOutput,
        queuedCommands: [],
        dryRun,
    };

    try {
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
    } finally {
        if (authoringClient != null) {
            await authoringClient.close();
        }
    }
}

async function runChat(
    authoringHost: string,
    project: string | undefined,
    jsonOutput: boolean,
    request: ChatRequest,
    options: ClientOptions,
): Promise<number> {
    return await withAuthoringHostClient(authoringHost, async (client) => {
        const controller = new AuthoringSessionController(client);
        const transcript = new AuthoringChatTranscript({ limit: 24 });
        const turn = new AuthoringTurn(controller);
        const rl = createInterface({ input, output });

        console.log("Luna chat session. Type '/help' for commands.");
        try {
            while (true) {
                let line: string;
                try {
                    line = (await rl.question("chat> ")).trim();
                } catch (error) {
                    if (isReadlineClosedError(error)) {
                        return 0;
                    }
                    throw error;
                }
                if (line.length === 0) {
                    continue;
                }

                try {
                    const status = await handleChatLine({
                        line,
                        controller,
                        transcript,
                        turn,
                        project,
                        jsonOutput,
                        request,
                        options,
                    });
                    if (status != null) {
                        return status;
                    }
                } catch (error) {
                    const message = error instanceof Error ? error.message : String(error);
                    console.error(message);
                    transcript.appendAssistant(JSON.stringify({
                        control: "error",
                        message,
                    }));
                }
            }
        } finally {
            rl.close();
        }
    });
}

function isReadlineClosedError(error: unknown): boolean {
    return error instanceof Error && error.message === "readline was closed";
}

type ChatLineContext = {
    line: string;
    controller: AuthoringSessionController;
    transcript: AuthoringChatTranscript;
    turn: AuthoringTurn;
    project?: string;
    jsonOutput: boolean;
    request: ChatRequest;
    options: ClientOptions;
};

async function handleChatLine(context: ChatLineContext): Promise<number | null> {
    const command = normalizeChatCommand(context.line);
    if (command === "exit" || command === "quit") {
        return 0;
    }
    if (command === "help") {
        printChatHelp();
        return null;
    }
    if (command === "history") {
        printChatHistory(context.transcript, context.jsonOutput);
        return null;
    }
    if (command === "state") {
        printChatSessionState(await context.controller.session());
        return null;
    }
    if (command === "snapshot") {
        const snapshot = await context.controller.snapshot();
        printChatSnapshot(snapshot.report, snapshot.session, context.jsonOutput);
        return null;
    }
    if (command === "undo") {
        const result = await context.controller.undo();
        printChatSessionState(result.session);
        return null;
    }
    if (command === "redo") {
        const result = await context.controller.redo();
        printChatSessionState(result.session);
        return null;
    }
    if (command === "clear") {
        context.transcript.clear();
        console.log("Conversation cleared.");
        return null;
    }
    if (command === "clear-history") {
        const result = await context.controller.clearHistory();
        printChatSessionState(result.session);
        return null;
    }

    const conversation = context.transcript.messages();
    const planner = createTurnPlanner(context.line, context.project, context.request, context.options);
    try {
        const result = await context.turn.run(context.line, planner, {
            project: context.project,
            transactionName: buildAuthoringTurnTransactionName(context.line),
            maxAttempts: context.request.maxAttempts,
            commitOnSuccess: context.request.execute,
            conversation,
        });

        context.transcript.appendUser(context.line);
        context.transcript.appendAssistant(renderAuthoringChatTurnSummary(result));
        printChatTurnResult(result, context.jsonOutput);
    } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        context.transcript.appendUser(context.line);
        context.transcript.appendAssistant(JSON.stringify({
            intent: context.line,
            ok: false,
            error: message,
        }));
        console.error(message);
    }
    return null;
}

function normalizeChatCommand(line: string): string {
    const trimmed = line.trim();
    if (trimmed.startsWith("/")) {
        return trimmed.slice(1).trim().toLowerCase();
    }

    const normalized = trimmed.toLowerCase();
    switch (normalized) {
        case "exit":
        case "quit":
        case "help":
        case "history":
        case "state":
        case "snapshot":
        case "undo":
        case "redo":
        case "clear":
        case "clear-history":
            return normalized;
        default:
            return "";
    }
}

function printChatHelp(): void {
    console.log(`Chat commands:
  /help           Show this help
  /history        Show the remembered conversation
  /state          Show the current session state
  /snapshot       Refresh and print the current report
  /undo           Undo the last committed authoring transaction
  /redo           Redo the last undone authoring transaction
  /clear          Clear conversation memory only
  /clear-history  Clear authoring undo/redo history
  /exit           Leave the chat

Any other line is sent to the authoring planner as a user instruction.`);
}

function printChatHistory(transcript: AuthoringChatTranscript, jsonOutput: boolean): void {
    const messages = transcript.messages();
    if (jsonOutput) {
        console.log(JSON.stringify(messages, null, 2));
        return;
    }

    if (messages.length === 0) {
        console.log("No conversation history.");
        return;
    }

    for (const [index, message] of messages.entries()) {
        console.log(`${index + 1}. ${message.role}: ${message.content}`);
    }
}

function printChatSessionState(session: Awaited<ReturnType<AuthoringSessionController["session"]>>): void {
    console.log(renderAuthoringSessionState(session));
}

function printChatSnapshot(
    report: AuthoringReport,
    session: Awaited<ReturnType<AuthoringSessionController["session"]>>,
    jsonOutput: boolean,
): void {
    if (jsonOutput) {
        console.log(JSON.stringify({ session, report }, null, 2));
        return;
    }

    console.log(renderAuthoringSessionState(session));
    printReportSummary(report);
}

function printChatTurnResult(result: AuthoringTurnResult, jsonOutput: boolean): void {
    if (jsonOutput) {
        console.log(JSON.stringify(result, null, 2));
        return;
    }

    console.log(renderAuthoringChatTurnSummary(result));
}

async function main(): Promise<number> {
    loadLunaEnvironment();
    const options = parseClientOptions(process.argv.slice(2));
    const command = options.passthrough[0];
    if (command == null || command === "--help" || command === "-h" || command === "help") {
        printUsage();
        return command == null ? 1 : 0;
    }

    const resolveLegacyHost = (): string =>
        options.host == null ? findDefaultLegacyHost() : resolve(options.host);
    const resolveAuthoringHost = (): string =>
        options.authoringHost == null ? findDefaultAuthoringHost() : resolve(options.authoringHost);
    let legacyHost: string | undefined;
    let hostArgs: string[];
    let project = options.project;

    if (command === "run") {
        legacyHost = resolveLegacyHost();
        hostArgs = options.passthrough.slice(1);
    } else if (command === "plan") {
        legacyHost = resolveLegacyHost();
        const planPath = options.passthrough[1];
        if (planPath == null) {
            throw new Error("Missing plan path.");
        }
        const plan = readAuthoringPlanJson(planPath);
        project = project ?? plan.project;
        hostArgs = ["plan", planPath];
    } else if (command === "capabilities") {
        if (options.passthrough.length !== 1) {
            throw new Error("Command 'capabilities' does not accept arguments.");
        }
        const authoringHost = resolveAuthoringHost();
        printCapabilities(await readCapabilitiesFromAuthoringHost(authoringHost));
        return 0;
    } else if (command === "compose") {
        const request = parseComposeRequest(options.passthrough.slice(1), options.execute);
        const authoringHost = resolveAuthoringHost();
        return await withAuthoringHostClient(authoringHost, async (client) => {
            const controller = new AuthoringSessionController(client);
            const capabilities = await controller.capabilities();
            const plan = composeLocalPlan(request.intent, capabilities, project);
            if (request.printPlanOnly) {
                printPlan(plan);
                return 0;
            }

            const result = await runComposeWithAuthoringController(controller, request.intent, plan, request.execute);
            console.log(JSON.stringify(result, null, 2));
            return result.ok ? 0 : 1;
        });
    } else if (command === "turn") {
        const request = parseTurnRequest(options.passthrough.slice(1), options.execute);
        const authoringHost = resolveAuthoringHost();
        return await withAuthoringHostClient(authoringHost, async (client) => {
            const controller = new AuthoringSessionController(client);
            const result = await runTurnWithAuthoringController(controller, request.intent, request, options, project);
            if (!request.printPlanOnly) {
                console.log(JSON.stringify(result, null, 2));
            }
            return result.ok ? 0 : 1;
        });
    } else if (command === "chat" || command === "session") {
        const request = parseChatRequest(options.passthrough.slice(1));
        const authoringHost = resolveAuthoringHost();
        return await runChat(authoringHost, project, options.jsonOutput, request, options);
    } else if (command === "eval") {
        const request = parseEvalRequest(options.passthrough.slice(1), options);
        const authoringHost = resolveAuthoringHost();
        return await withAuthoringHostClient(authoringHost, async (client) => {
            const controller = new AuthoringSessionController(client);
            return await runEvalWithAuthoringController(controller, request, options, project);
        });
    } else if (command === "eval-suite") {
        const request = parseEvalSuiteRequest(options.passthrough.slice(1), options);
        const authoringHost = resolveAuthoringHost();
        return await runEvalSuite(authoringHost, request, options, project);
    } else if (command === "interactive") {
        const interactiveLegacyHost = options.dryRun ? resolveLegacyHost() : undefined;
        const authoringHost = resolveAuthoringHost();
        return await runInteractive(interactiveLegacyHost, authoringHost, project, options.jsonOutput, options.dryRun);
    } else {
        throw new Error(`Unknown TS client command '${command}'. Use 'run', 'plan', 'capabilities', 'compose', 'turn', 'chat', 'session', 'eval', 'eval-suite', or 'interactive'.`);
    }

    if (project != null) {
        hostArgs = ["--project", project, ...hostArgs];
    }
    if (options.jsonOutput) {
        hostArgs = ["--json", ...hostArgs];
    }

    if (legacyHost == null) {
        throw new Error("Legacy LunaCLI host is required for this command.");
    }
    return runHost(legacyHost, hostArgs, options.dryRun);
}

try {
    process.exitCode = await main();
} catch (error) {
    console.error(error instanceof Error ? error.message : String(error));
    process.exitCode = 1;
}
