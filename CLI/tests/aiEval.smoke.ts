#!/usr/bin/env node

import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { createServer, type IncomingMessage, type ServerResponse } from "node:http";
import { existsSync, mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";

const [hostPath, clientPath, ...evalPaths] = process.argv.slice(2);
assert.ok(hostPath, "Missing LunaAuthoringHost path.");
assert.ok(clientPath, "Missing TS client path.");
assert.ok(evalPaths.length > 0, "Missing authoring eval path.");

const outputRoot = mkdtempSync(join(tmpdir(), "luna-ai-eval-"));
let requestCount = 0;

const server = createServer((request: IncomingMessage, response: ServerResponse) => {
    void handleMockRequest(request, response).catch((error) => {
        response.statusCode = 500;
        response.setHeader("Content-Type", "text/plain; charset=utf-8");
        response.end(error instanceof Error ? error.stack ?? error.message : String(error));
    });
});

try {
    await listen(server);
    const address = server.address();
    assert.ok(address != null && typeof address === "object");
    const baseURL = `http://127.0.0.1:${address.port}`;

    for (const evalPath of evalPaths) {
        const result = await runClient([
            "--disable-warning=ExperimentalWarning",
            "--experimental-strip-types",
            resolve(clientPath),
            "--authoring-host",
            resolve(hostPath),
            "--json",
            "--ai",
            "--ai-base-url",
            baseURL,
            "--ai-api-key",
            "mock-key",
            "--ai-model",
            "mock-model",
            "eval",
            resolve(evalPath),
            "--out-dir",
            outputRoot,
        ]);

        assert.equal(result.status, 0, `${evalPath}\nstdout=${result.stdout}\nstderr=${result.stderr}`);
        const output = JSON.parse(result.stdout) as {
            ok: boolean;
            resultPath: string;
            turns: Array<{
                ok: boolean;
                failures: string[];
                kind: string;
                plan?: { commands: Array<{ op: string }> };
                turn?: { report: { savedScenes: string[] }; attempts: unknown[] };
                evaluationReport?: { scene: { entityCount: number } };
            }>;
        };
        assert.equal(output.ok, true, evalPath);
        assert.ok(existsSync(output.resultPath), output.resultPath);
        assert.ok(output.turns.length > 0);
        for (const turn of output.turns) {
            assert.equal(turn.ok, true, `${evalPath}: ${turn.failures.join("\n")}`);
            assert.deepEqual(turn.failures, []);
        }
    }

    const suite = await runClient([
        "--disable-warning=ExperimentalWarning",
        "--experimental-strip-types",
        resolve(clientPath),
        "--authoring-host",
        resolve(hostPath),
        "--json",
        "--ai",
        "--ai-base-url",
        baseURL,
        "--ai-api-key",
        "mock-key",
        "--ai-model",
        "mock-model",
        "eval-suite",
        ...evalPaths.map((evalPath) => resolve(evalPath)),
        "--out-dir",
        outputRoot,
    ]);
    assert.equal(suite.status, 0, `eval-suite\nstdout=${suite.stdout}\nstderr=${suite.stderr}`);
    const suiteOutput = JSON.parse(suite.stdout) as {
        ok: boolean;
        resultPath: string;
        evals: Array<{ ok: boolean; name: string; turnCount: number; failedTurns: unknown[] }>;
    };
    assert.equal(suiteOutput.ok, true);
    assert.ok(existsSync(suiteOutput.resultPath));
    assert.equal(suiteOutput.evals.length, evalPaths.length);
    for (const entry of suiteOutput.evals) {
        assert.equal(entry.ok, true, entry.name);
        assert.ok(entry.turnCount > 0);
        assert.deepEqual(entry.failedTurns, []);
    }

    assert.ok(requestCount >= evalPaths.length);
} finally {
    await close(server);
}

function readRequestBody(request: IncomingMessage): Promise<string> {
    return new Promise((resolveBody, reject) => {
        let body = "";
        request.setEncoding("utf8");
        request.on("data", (chunk) => {
            body += chunk;
        });
        request.on("end", () => resolveBody(body));
        request.on("error", reject);
    });
}

async function handleMockRequest(request: IncomingMessage, response: ServerResponse): Promise<void> {
    requestCount += 1;
    assert.equal(request.method, "POST");
    assert.equal(request.url, "/chat/completions");
    assert.equal(request.headers.authorization, "Bearer mock-key");

    const body = JSON.parse(await readRequestBody(request)) as {
        model: string;
        stream: boolean;
        messages: Array<{ role: string; content: string }>;
    };
    assert.equal(body.model, "mock-model");
    assert.equal(body.stream, false);
    assert.equal(body.messages[0]?.role, "system");
    assert.equal(body.messages.at(-1)?.role, "user");

    const plannerContext = JSON.parse(body.messages.at(-1)?.content ?? "{}") as {
        intent?: string;
        attempt?: number;
        previousAttempt?: unknown;
    };
    const intent = plannerContext.intent ?? "";
    assert.match(intent, /snapshot|repair|检查/);

    const plan = createMockPlan(intent, plannerContext.attempt ?? 1);
    response.writeHead(200, { "Content-Type": "application/json" });
    response.end(JSON.stringify({
        id: "mock-ai-eval",
        object: "chat.completion",
        choices: [
            {
                index: 0,
                message: {
                    role: "assistant",
                    content: JSON.stringify(plan),
                },
                finish_reason: "stop",
            },
        ],
    }));
}

function createMockPlan(intent: string, attempt: number): { commands: Array<Record<string, unknown>> } {
    if (intent.includes("Repair the failed previous attempt")) {
        assert.equal(attempt, 2);
        return {
            commands: [
                { op: "new" },
                { op: "primitive", alias: "Cube", mesh: "Cube" },
                { op: "snapshot" },
            ],
        };
    }

    if (intent.includes("ground plane") || intent.includes("地面")) {
        return basicScenePlan(extractSavePath(intent));
    }
    if (intent.includes("persist-scene") && intent.includes("Create a new scene")) {
        return oneCubeCameraLightPlan(extractSavePath(intent), true);
    }
    if (intent.includes("Open the saved scene")) {
        return {
            commands: [
                { op: "open", path: extractOpenPath(intent) },
                { op: "snapshot" },
            ],
        };
    }
    if (intent.includes("Start a new scene")) {
        return oneCubeCameraLightPlan(undefined, false);
    }
    if (intent.includes("Create a new scene with one cube")) {
        return {
            commands: [
                { op: "new" },
                { op: "primitive", alias: "Cube", mesh: "Cube" },
                { op: "snapshot" },
            ],
        };
    }
    if (intent.includes("Add one sphere and one point light")) {
        return {
            commands: [
                { op: "primitive", alias: "Sphere", mesh: "Sphere" },
                { op: "point-light", alias: "FillLight" },
                { op: "snapshot" },
            ],
        };
    }
    if (intent.includes("Add one sphere")) {
        return {
            commands: [
                { op: "primitive", alias: "Sphere", mesh: "Sphere" },
                { op: "snapshot" },
            ],
        };
    }
    if (intent.includes("Move the existing cube")) {
        return {
            commands: [
                { op: "transform", entity: "Cube", translation: [2, 1, 0], rotationDeg: [0, 0, 0], scale: [1, 1, 1] },
                { op: "snapshot" },
            ],
        };
    }

    throw new Error(`No mock plan for intent: ${intent}`);
}

function basicScenePlan(savePath: string): { commands: Array<Record<string, unknown>> } {
    return {
        commands: [
            { op: "new" },
            { op: "primitive", alias: "Floor", mesh: "Plane" },
            { op: "transform", entity: "Floor", translation: [0, -0.5, 0], scale: [8, 1, 8] },
            { op: "primitive", alias: "Cube", mesh: "Cube" },
            { op: "camera", alias: "MainCamera" },
            { op: "transform", entity: "MainCamera", translation: [0, 3, 7], rotationDeg: [-20, 0, 0] },
            { op: "camera-perspective", entity: "MainCamera", fovDeg: 60, near: 0.1, far: 500 },
            { op: "point-light", alias: "KeyLight" },
            { op: "transform", entity: "KeyLight", translation: [2, 4, 2] },
            { op: "light-intensity", entity: "KeyLight", value: 3.5 },
            { op: "save", path: savePath },
            { op: "snapshot" },
        ],
    };
}

function oneCubeCameraLightPlan(savePath: string | undefined, save: boolean): { commands: Array<Record<string, unknown>> } {
    const commands: Array<Record<string, unknown>> = [
        { op: "new" },
        { op: "primitive", alias: "Cube", mesh: "Cube" },
        { op: "camera", alias: "MainCamera" },
        { op: "transform", entity: "MainCamera", translation: [0, 3, 7], rotationDeg: [-20, 0, 0] },
        { op: "camera-perspective", entity: "MainCamera", fovDeg: 60, near: 0.1, far: 500 },
        { op: "point-light", alias: "KeyLight" },
        { op: "transform", entity: "KeyLight", translation: [2, 4, 2] },
    ];
    if (save) {
        commands.push({ op: "save", path: assertString(savePath, "savePath") });
    }
    commands.push({ op: "snapshot" });
    return { commands };
}

function extractSavePath(intent: string): string {
    const match = /Save it to\s+(\S+)/i.exec(intent) ?? /保存到\s+([^，,\s]+)/.exec(intent);
    assert.ok(match?.[1], `Could not find save path in intent: ${intent}`);
    return match[1];
}

function extractOpenPath(intent: string): string {
    const match = /Open the saved scene at\s+(\S+)/i.exec(intent);
    assert.ok(match?.[1], `Could not find open path in intent: ${intent}`);
    return match[1];
}

function assertString(value: string | undefined, name: string): string {
    assert.ok(value, `Missing ${name}.`);
    return value;
}

function runClient(args: string[]): Promise<{ status: number; stdout: string; stderr: string }> {
    return new Promise((resolveRun, reject) => {
        const child = spawn(process.execPath, args, {
            stdio: ["ignore", "pipe", "pipe"],
        });

        let stdout = "";
        let stderr = "";
        child.stdout.setEncoding("utf8");
        child.stderr.setEncoding("utf8");
        child.stdout.on("data", (chunk) => {
            stdout += chunk;
        });
        child.stderr.on("data", (chunk) => {
            stderr += chunk;
        });
        child.once("error", reject);
        child.once("close", (status) => {
            resolveRun({
                status: status ?? 1,
                stdout,
                stderr,
            });
        });
    });
}

function listen(serverToListen: ReturnType<typeof createServer>): Promise<void> {
    return new Promise((resolveListen, reject) => {
        serverToListen.once("error", reject);
        serverToListen.listen(0, "127.0.0.1", () => {
            serverToListen.off("error", reject);
            resolveListen();
        });
    });
}

function close(serverToClose: ReturnType<typeof createServer>): Promise<void> {
    return new Promise((resolveClose, reject) => {
        serverToClose.close((error) => {
            if (error != null) {
                reject(error);
                return;
            }
            resolveClose();
        });
    });
}
