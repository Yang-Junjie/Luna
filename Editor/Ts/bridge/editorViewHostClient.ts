import { spawn, type ChildProcessWithoutNullStreams } from "node:child_process";
import { createInterface, type Interface } from "node:readline";

import type { EditorViewportId } from "../protocol/editorProtocol.ts";
import type { EditorRenderPlaneDescriptor } from "../protocol/renderDataPlane.ts";
import type { EditorViewportCommand } from "../protocol/viewProtocol.ts";
import {
    normalizeEditorRenderPlaneResult,
    normalizeEditorViewBridgeCommandResult,
    normalizeEditorViewBridgeDocument,
    type EditorRenderFramePresentation,
    type EditorViewBridge,
    type EditorViewBridgeCommandResult,
    type EditorViewBridgeDocument,
    type EditorViewBridgeRenderPlaneResult,
} from "./editorViewBridge.ts";

export type EditorViewHostClientOptions = {
    timeoutMs?: number;
};

type RpcErrorPayload = {
    code: number;
    message: string;
    data?: unknown;
};

type RpcResponse = {
    jsonrpc: "2.0";
    id: number | string | null;
    result?: unknown;
    error?: RpcErrorPayload;
};

type PendingRequest = {
    resolve: (response: RpcResponse) => void;
    reject: (error: Error) => void;
    timer: ReturnType<typeof setTimeout> | null;
};

export class EditorViewHostRpcError extends Error {
    readonly code: number;
    readonly data: unknown;

    constructor(error: RpcErrorPayload) {
        super(`${error.code}: ${error.message}`);
        this.name = "EditorViewHostRpcError";
        this.code = error.code;
        this.data = error.data;
    }
}

export class EditorViewHostClient implements EditorViewBridge {
    #process: ChildProcessWithoutNullStreams;
    #lines: Interface;
    #nextId = 1;
    #pending = new Map<number, PendingRequest>();
    #stderr = "";
    #closed = false;
    #exited = false;
    #timeoutMs: number;

    constructor(hostPath: string, options: EditorViewHostClientOptions = {}) {
        this.#timeoutMs = options.timeoutMs ?? 5000;
        this.#process = spawn(hostPath, [], { stdio: ["pipe", "pipe", "pipe"] });
        this.#lines = createInterface({ input: this.#process.stdout });
        this.#lines.on("line", (line) => this.#handleLine(line));
        this.#process.stderr.on("data", (chunk) => this.#appendStderr(chunk));
        this.#process.on("error", (error) => {
            this.#closed = true;
            this.#rejectAll(error);
        });
        this.#process.on("exit", (code, signal) => {
            this.#closed = true;
            this.#exited = true;
            if (this.#pending.size === 0) {
                return;
            }

            const reason = signal == null
                ? `Editor view host exited with code ${code ?? "unknown"}.`
                : `Editor view host exited due to signal ${signal}.`;
            this.#rejectAll(new Error(this.#withStderr(reason)));
        });
    }

    async snapshot(): Promise<EditorViewBridgeDocument> {
        return normalizeEditorViewBridgeDocument(await this.request("snapshot"));
    }

    async applyViewportCommand(command: EditorViewportCommand): Promise<EditorViewBridgeCommandResult> {
        return normalizeEditorViewBridgeCommandResult(await this.request("applyViewportCommand", { command }));
    }

    async bindRenderPlane(
        viewportId: EditorViewportId,
        descriptor: EditorRenderPlaneDescriptor,
    ): Promise<EditorViewBridgeRenderPlaneResult> {
        return normalizeEditorRenderPlaneResult(
            await this.request("bindRenderPlane", { viewportId, descriptor }),
        );
    }

    async presentRenderPlaneFrame(
        viewportId: EditorViewportId,
        frame: EditorRenderFramePresentation,
    ): Promise<EditorViewBridgeRenderPlaneResult> {
        return normalizeEditorRenderPlaneResult(
            await this.request("presentRenderPlaneFrame", { viewportId, ...frame }),
        );
    }

    async releaseRenderPlane(viewportId: EditorViewportId): Promise<EditorViewBridgeRenderPlaneResult> {
        return normalizeEditorRenderPlaneResult(await this.request("releaseRenderPlane", { viewportId }));
    }

    async shutdown(): Promise<boolean> {
        const result = requireRecord(await this.request("shutdown"), "shutdownResult");
        return requireBoolean(result.ok, "shutdownResult.ok");
    }

    async close(): Promise<void> {
        if (this.#closed && this.#pending.size === 0 && this.#exited) {
            return;
        }

        let shutdownOk = false;
        try {
            if (!this.#closed) {
                shutdownOk = await this.shutdown();
            }
        } catch {
            // close() is best-effort cleanup; callers can use shutdown() when they need the result.
        } finally {
            this.#closed = true;
            this.#lines.close();
            this.#process.stdin.end();
            if (shutdownOk && !this.#exited) {
                await this.#waitForExit(500);
            }
            if (!shutdownOk && !this.#exited) {
                this.#process.kill();
            }
        }
    }

    async request(method: string, params?: unknown): Promise<unknown> {
        if (this.#closed) {
            throw new Error(this.#withStderr("Editor view host is closed."));
        }

        const id = this.#nextId++;
        const responsePromise = new Promise<RpcResponse>((resolve, reject) => {
            const timer = setTimeout(() => {
                this.#pending.delete(id);
                reject(new Error(this.#withStderr(`Editor view host request '${method}' timed out.`)));
            }, this.#timeoutMs);
            this.#pending.set(id, { resolve, reject, timer });
        });

        const request = params == null
            ? { jsonrpc: "2.0", id, method }
            : { jsonrpc: "2.0", id, method, params };
        this.#process.stdin.write(`${JSON.stringify(request)}\n`);

        const response = await responsePromise;
        if (response.error != null) {
            throw new EditorViewHostRpcError(response.error);
        }
        return response.result;
    }

    #handleLine(line: string): void {
        let response: RpcResponse;
        try {
            response = JSON.parse(line) as RpcResponse;
        } catch (error) {
            this.#rejectAll(error instanceof Error ? error : new Error(String(error)));
            return;
        }

        if (typeof response.id !== "number") {
            this.#rejectAll(new Error(`Editor view host returned a non-numeric id: ${JSON.stringify(response.id)}.`));
            return;
        }

        const pending = this.#pending.get(response.id);
        if (pending == null) {
            return;
        }
        this.#pending.delete(response.id);
        if (pending.timer != null) {
            clearTimeout(pending.timer);
        }
        pending.resolve(response);
    }

    #appendStderr(chunk: unknown): void {
        this.#stderr += Buffer.isBuffer(chunk) ? chunk.toString("utf8") : String(chunk);
        if (this.#stderr.length > 8192) {
            this.#stderr = this.#stderr.slice(-8192);
        }
    }

    #rejectAll(error: Error): void {
        for (const pending of this.#pending.values()) {
            if (pending.timer != null) {
                clearTimeout(pending.timer);
            }
            pending.reject(error);
        }
        this.#pending.clear();
    }

    #withStderr(message: string): string {
        const stderr = this.#stderr.trim();
        return stderr.length === 0 ? message : `${message}\nstderr:\n${stderr}`;
    }

    #waitForExit(timeoutMs: number): Promise<void> {
        if (this.#exited) {
            return Promise.resolve();
        }

        return new Promise((resolve) => {
            const timer = setTimeout(() => {
                this.#process.off("exit", onExit);
                resolve();
            }, timeoutMs);
            const onExit = () => {
                clearTimeout(timer);
                resolve();
            };
            this.#process.once("exit", onExit);
        });
    }
}

function requireRecord(value: unknown, field: string): Record<string, unknown> {
    if (value == null || typeof value !== "object" || Array.isArray(value)) {
        throw new Error(`Field '${field}' must be an object.`);
    }
    return value as Record<string, unknown>;
}

function requireBoolean(value: unknown, field: string): boolean {
    if (typeof value !== "boolean") {
        throw new Error(`Field '${field}' must be a boolean.`);
    }
    return value;
}
