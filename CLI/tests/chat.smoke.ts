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
        "chat",
    ], [
        "create a cube",
        "add a sphere to the scene",
        "exit",
    ].join("\n") + "\n");

    assert.equal(result.status, 0, result.stderr);
    assert.equal(requestCount, 2);
    assert.match(result.stdout, /"intent":"create a cube"/);
    assert.match(result.stdout, /"intent":"add a sphere to the scene"/);
    assert.match(result.stdout, /"entityCount":4/);
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
        messages: Array<{ role: string; content: string }>;
    };
    assert.equal(body.model, "mock-model");
    assert.equal(body.stream, false);

    if (requestCount === 1) {
        assert.equal(body.messages.length, 2);
        assert.equal(body.messages[0]?.role, "system");
        assert.equal(body.messages[1]?.role, "user");
        assert.match(body.messages[1]?.content ?? "", /"intent":"create a cube"/);
    } else if (requestCount === 2) {
        assert.equal(body.messages.length, 4);
        assert.equal(body.messages[1]?.role, "user");
        assert.match(body.messages[1]?.content ?? "", /create a cube/);
        assert.equal(body.messages[2]?.role, "assistant");
        assert.match(body.messages[2]?.content ?? "", /"entityCount":3/);
        assert.equal(body.messages[3]?.role, "user");
        assert.match(body.messages[3]?.content ?? "", /add a sphere to the scene/);
    } else {
        throw new Error(`Unexpected request count ${requestCount}.`);
    }

    const assistantContent = requestCount === 1
        ? JSON.stringify({
            commands: [
                { op: "new" },
                { op: "primitive", alias: "ChatCube", mesh: "Cube" },
                { op: "snapshot" },
            ],
        })
        : JSON.stringify({
            commands: [
                { op: "primitive", alias: "ChatSphere", mesh: "Sphere" },
                { op: "snapshot" },
            ],
        });

    response.writeHead(200, { "Content-Type": "application/json" });
    response.end(JSON.stringify({
        id: `mock-chatcmpl-${requestCount}`,
        object: "chat.completion",
        choices: [
            {
                index: 0,
                message: {
                    role: "assistant",
                    content: assistantContent,
                },
                finish_reason: "stop",
            },
        ],
    }));
}

async function runClient(args: string[]): Promise<{ status: number; stdout: string; stderr: string }> {
    const child = spawn(process.execPath, args, {
        stdio: ["pipe", "pipe", "pipe"],
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

    const closePromise = new Promise<number>((resolve, reject) => {
        child.once("error", reject);
        child.once("close", (status) => {
            resolve(status ?? 1);
        });
    });

    try {
        await driveChat(child, () => stdout);
    } catch (error) {
        child.kill();
        throw error;
    }

    const status = await closePromise;
    return { status, stdout, stderr };
}

async function driveChat(child: ReturnType<typeof spawn>, readStdout: () => string): Promise<void> {
    await waitForText(readStdout, "chat> ");
    child.stdin.write("create a cube\n");

    await waitForText(readStdout, "\"entityCount\":3");
    child.stdin.write("add a sphere to the scene\n");

    await waitForText(readStdout, "\"entityCount\":4");
    child.stdin.write("exit\n");
    child.stdin.end();
}

async function waitForText(readText: () => string, marker: string, timeoutMs = 10000): Promise<void> {
    const started = Date.now();
    while (Date.now() - started < timeoutMs) {
        if (readText().includes(marker)) {
            return;
        }
        await delay(25);
    }
    throw new Error(`Timed out waiting for '${marker}'.`);
}

function delay(ms: number): Promise<void> {
    return new Promise((resolve) => {
        setTimeout(resolve, ms);
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
