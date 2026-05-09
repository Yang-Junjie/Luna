#!/usr/bin/env node

import assert from "node:assert/strict";
import { createServer, type IncomingMessage, type ServerResponse } from "node:http";
import { spawn } from "node:child_process";

const [hostPath, clientPath] = process.argv.slice(2);
assert.ok(hostPath, "Missing LunaAuthoringHost path.");
assert.ok(clientPath, "Missing TS client path.");

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

    const result = await runClient([
        "--disable-warning=ExperimentalWarning",
        "--experimental-strip-types",
        clientPath,
        "--authoring-host",
        hostPath,
        "--ai",
        "--ai-base-url",
        baseURL,
        "--ai-api-key",
        "mock-key",
        "--ai-model",
        "mock-model",
        "--ai-thinking",
        "--ai-reasoning-effort",
        "high",
        "turn",
        "ask the model to create one cube",
        "--execute",
    ]);

    assert.equal(result.status, 0, result.stderr);
    assert.equal(requestCount, 1);
    const output = JSON.parse(result.stdout) as {
        ok: boolean;
        committed: boolean;
        attempts: Array<{ plan: { commands: Array<Record<string, unknown>> } }>;
        report: { ok: boolean; scene: { entityCount: number } };
    };
    assert.equal(output.ok, true);
    assert.equal(output.committed, true);
    assert.equal(output.report.ok, true);
    assert.equal(output.report.scene.entityCount, 3);
    assert.ok(output.attempts[0]?.plan.commands.some((command) => command.op === "primitive" && command.alias === "AICube"));
} finally {
    await close(server);
}

function readRequestBody(request: IncomingMessage): Promise<string> {
    return new Promise((resolve, reject) => {
        let body = "";
        request.setEncoding("utf8");
        request.on("data", (chunk) => {
            body += chunk;
        });
        request.on("end", () => resolve(body));
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
        reasoning_effort?: string;
        thinking?: { type?: string };
        messages: Array<{ role: string; content: string }>;
    };
    assert.equal(body.model, "mock-model");
    assert.equal(body.stream, false);
    assert.equal(body.reasoning_effort, "high");
    assert.equal(body.thinking?.type, "enabled");
    assert.equal(body.messages[0]?.role, "system");
    assert.equal(body.messages[1]?.role, "user");

    response.writeHead(200, { "Content-Type": "application/json" });
    response.end(JSON.stringify({
        id: "mock-chatcmpl",
        object: "chat.completion",
        choices: [
            {
                index: 0,
                message: {
                    role: "assistant",
                    content: JSON.stringify({
                        commands: [
                            { op: "new" },
                            { op: "primitive", alias: "AICube", mesh: "Cube" },
                            { op: "inspect", target: "hierarchy" },
                            { op: "snapshot" },
                        ],
                    }),
                },
                finish_reason: "stop",
            },
        ],
    }));
}

function runClient(args: string[]): Promise<{ status: number; stdout: string; stderr: string }> {
    return new Promise((resolve, reject) => {
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
            resolve({
                status: status ?? 1,
                stdout,
                stderr,
            });
        });
    });
}

function listen(serverToListen: ReturnType<typeof createServer>): Promise<void> {
    return new Promise((resolve, reject) => {
        serverToListen.once("error", reject);
        serverToListen.listen(0, "127.0.0.1", () => {
            serverToListen.off("error", reject);
            resolve();
        });
    });
}

function close(serverToClose: ReturnType<typeof createServer>): Promise<void> {
    return new Promise((resolve, reject) => {
        serverToClose.close((error) => {
            if (error != null) {
                reject(error);
                return;
            }
            resolve();
        });
    });
}
