import { spawn, type ChildProcessWithoutNullStreams } from "node:child_process";
import { createInterface, type Interface } from "node:readline";

import {
    normalizeAuthoringCapabilities,
    normalizeAuthoringPlan,
    normalizeAuthoringReport,
    normalizeAuthoringSceneSnapshot,
    requireNonNegativeInteger,
    requireString,
    requireNonEmptyString,
    type AuthoringCapabilitiesDocument,
    type AuthoringPlan,
    type AuthoringReport,
    type AuthoringSceneSnapshot,
} from "./authoringProtocol.ts";

export type AuthoringHostClientOptions = {
    timeoutMs?: number;
};

export type AuthoringHostEventType =
    | "sceneReset"
    | "sceneCreated"
    | "sceneLoaded"
    | "sceneSaved"
    | "sceneDirtyChanged"
    | "sceneSettingsChanged"
    | "historyChanged"
    | "entityCreated"
    | "entityModified"
    | "entityDestroyed"
    | "entityReparented"
    | "componentAdded"
    | "componentRemoved"
    | "unknown";

export type AuthoringHostEvent = {
    type: AuthoringHostEventType;
    entity: string | null;
    path: string | null;
    message: string;
};

export type AuthoringHostSessionState = {
    hasScene: boolean;
    scene: AuthoringSceneSnapshot;
    hasOpenTransaction: boolean;
    canUndo: boolean;
    canRedo: boolean;
    undoDepth: number;
    redoDepth: number;
};

export type AuthoringHostSessionResult = {
    ok: boolean;
    session: AuthoringHostSessionState;
};

export type AuthoringHostMutationResult = {
    ok: boolean;
    scene: AuthoringSceneSnapshot;
    events: AuthoringHostEvent[];
};

export type AuthoringHostExecuteResult = {
    ok: boolean;
    report: AuthoringReport;
    events: AuthoringHostEvent[];
};

export type AuthoringHostUndoRedoResult = {
    ok: boolean;
    scene: AuthoringSceneSnapshot;
    events: AuthoringHostEvent[];
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

export class AuthoringHostRpcError extends Error {
    readonly code: number;
    readonly data: unknown;

    constructor(error: RpcErrorPayload) {
        super(`${error.code}: ${error.message}`);
        this.name = "AuthoringHostRpcError";
        this.code = error.code;
        this.data = error.data;
    }
}

export class AuthoringHostClient {
    #process: ChildProcessWithoutNullStreams;
    #lines: Interface;
    #nextId = 1;
    #pending = new Map<number, PendingRequest>();
    #stderr = "";
    #closed = false;
    #exited = false;
    #timeoutMs: number;

    constructor(hostPath: string, options: AuthoringHostClientOptions = {}) {
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
                ? `Authoring host exited with code ${code ?? "unknown"}.`
                : `Authoring host exited due to signal ${signal}.`;
            this.#rejectAll(new Error(this.#withStderr(reason)));
        });
    }

    async capabilities(): Promise<AuthoringCapabilitiesDocument> {
        return normalizeAuthoringCapabilities(await this.request("capabilities"));
    }

    async session(): Promise<AuthoringHostSessionState> {
        return normalizeSessionState(await this.request("session"));
    }

    async beginTransaction(name: string): Promise<AuthoringHostSessionResult> {
        return normalizeSessionResult(await this.request("beginTransaction", { name: requireNonEmptyString(name, "name") }));
    }

    async commitTransaction(): Promise<AuthoringHostMutationResult> {
        return normalizeMutationResult(await this.request("commitTransaction"));
    }

    async rollbackTransaction(): Promise<AuthoringHostMutationResult> {
        return normalizeMutationResult(await this.request("rollbackTransaction"));
    }

    async executePlan(plan: AuthoringPlan): Promise<AuthoringHostExecuteResult> {
        return normalizeExecuteResult(await this.request("executePlan", { plan: normalizeAuthoringPlan(plan) }));
    }

    async snapshot(): Promise<AuthoringHostExecuteResult> {
        return normalizeExecuteResult(await this.request("snapshot"));
    }

    async undo(): Promise<AuthoringHostUndoRedoResult> {
        return normalizeUndoRedoResult(await this.request("undo"), "undo");
    }

    async redo(): Promise<AuthoringHostUndoRedoResult> {
        return normalizeUndoRedoResult(await this.request("redo"), "redo");
    }

    async events(): Promise<AuthoringHostEvent[]> {
        const result = requireRecord(await this.request("events"), "eventsResult");
        return normalizeEvents(result.events, "eventsResult.events");
    }

    async clearAliases(): Promise<boolean> {
        return requireOkResult(await this.request("clearAliases"), "clearAliasesResult");
    }

    async clearHistory(): Promise<AuthoringHostSessionResult> {
        return normalizeSessionResult(await this.request("clearHistory"));
    }

    async shutdown(): Promise<boolean> {
        return requireOkResult(await this.request("shutdown"), "shutdownResult");
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
            throw new Error("Authoring host is closed.");
        }

        const id = this.#nextId++;
        const request = params == null
            ? { jsonrpc: "2.0", id, method }
            : { jsonrpc: "2.0", id, method, params };

        const response = await new Promise<RpcResponse>((resolve, reject) => {
            const timer = this.#timeoutMs > 0
                ? setTimeout(() => {
                    this.#pending.delete(id);
                    reject(new Error(this.#withStderr(`Authoring host request '${method}' timed out.`)));
                }, this.#timeoutMs)
                : null;
            timer?.unref?.();

            this.#pending.set(id, {
                resolve,
                reject,
                timer,
            });

            this.#process.stdin.write(`${JSON.stringify(request)}\n`, (error) => {
                if (error == null) {
                    return;
                }

                const pending = this.#pending.get(id);
                if (pending == null) {
                    return;
                }
                this.#pending.delete(id);
                clearPendingTimer(pending);
                reject(error);
            });
        });

        if (response.error != null) {
            throw new AuthoringHostRpcError(response.error);
        }
        return response.result;
    }

    #handleLine(line: string): void {
        let response: RpcResponse;
        try {
            response = JSON.parse(line) as RpcResponse;
        } catch (error) {
            this.#rejectAll(new Error(`Authoring host returned invalid JSON: ${errorMessage(error)}`));
            return;
        }

        if (response.jsonrpc !== "2.0" || typeof response.id !== "number") {
            this.#rejectAll(new Error(`Authoring host returned an invalid response id: ${String(response.id)}`));
            return;
        }

        const pending = this.#pending.get(response.id);
        if (pending == null) {
            this.#rejectAll(new Error(`Authoring host returned an unexpected response id: ${response.id}`));
            return;
        }

        this.#pending.delete(response.id);
        clearPendingTimer(pending);
        pending.resolve(response);
    }

    #appendStderr(chunk: Buffer | string): void {
        this.#stderr += chunk.toString();
        if (this.#stderr.length > 16 * 1024) {
            this.#stderr = this.#stderr.slice(-16 * 1024);
        }
    }

    #rejectAll(error: Error): void {
        this.#closed = true;
        for (const pending of this.#pending.values()) {
            clearPendingTimer(pending);
            pending.reject(error);
        }
        this.#pending.clear();
    }

    #withStderr(message: string): string {
        const stderr = this.#stderr.trim();
        return stderr.length === 0 ? message : `${message}\n${stderr}`;
    }

    async #waitForExit(timeoutMs: number): Promise<void> {
        if (this.#exited) {
            return;
        }

        await new Promise<void>((resolve) => {
            const timer = setTimeout(resolve, timeoutMs);
            timer.unref?.();
            this.#process.once("exit", () => {
                clearTimeout(timer);
                resolve();
            });
        });
    }
}

function clearPendingTimer(pending: PendingRequest): void {
    if (pending.timer != null) {
        clearTimeout(pending.timer);
    }
}

function errorMessage(error: unknown): string {
    return error instanceof Error ? error.message : String(error);
}

function requireRecord(value: unknown, field: string): Record<string, unknown> {
    if (typeof value !== "object" || value == null || Array.isArray(value)) {
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

function nullableString(value: unknown, field: string): string | null {
    if (value == null) {
        return null;
    }
    return requireString(value, field);
}

function requireOkResult(value: unknown, field: string): boolean {
    const record = requireRecord(value, field);
    return requireBoolean(record.ok, `${field}.ok`);
}

function normalizeSessionState(value: unknown): AuthoringHostSessionState {
    const record = requireRecord(value, "sessionState");
    return {
        hasScene: requireBoolean(record.hasScene, "sessionState.hasScene"),
        scene: normalizeAuthoringSceneSnapshot(record.scene, "sessionState.scene"),
        hasOpenTransaction: requireBoolean(record.hasOpenTransaction, "sessionState.hasOpenTransaction"),
        canUndo: requireBoolean(record.canUndo, "sessionState.canUndo"),
        canRedo: requireBoolean(record.canRedo, "sessionState.canRedo"),
        undoDepth: requireNonNegativeInteger(record.undoDepth, "sessionState.undoDepth"),
        redoDepth: requireNonNegativeInteger(record.redoDepth, "sessionState.redoDepth"),
    };
}

function normalizeSessionResult(value: unknown): AuthoringHostSessionResult {
    const record = requireRecord(value, "sessionResult");
    return {
        ok: requireBoolean(record.ok, "sessionResult.ok"),
        session: normalizeSessionState(record.session),
    };
}

function normalizeMutationResult(value: unknown): AuthoringHostMutationResult {
    const record = requireRecord(value, "mutationResult");
    return {
        ok: requireBoolean(record.ok, "mutationResult.ok"),
        scene: normalizeAuthoringSceneSnapshot(record.scene, "mutationResult.scene"),
        events: normalizeEvents(record.events, "mutationResult.events"),
    };
}

function normalizeExecuteResult(value: unknown): AuthoringHostExecuteResult {
    const record = requireRecord(value, "executeResult");
    return {
        ok: requireBoolean(record.ok, "executeResult.ok"),
        report: normalizeAuthoringReport(record.report),
        events: normalizeEvents(record.events, "executeResult.events"),
    };
}

function normalizeUndoRedoResult(value: unknown, label: string): AuthoringHostUndoRedoResult {
    const record = requireRecord(value, `${label}Result`);
    return {
        ok: requireBoolean(record.ok, `${label}Result.ok`),
        scene: normalizeAuthoringSceneSnapshot(record.scene, `${label}Result.scene`),
        events: normalizeEvents(record.events, `${label}Result.events`),
    };
}

function normalizeEvents(value: unknown, field: string): AuthoringHostEvent[] {
    if (!Array.isArray(value)) {
        throw new Error(`Field '${field}' must be an array.`);
    }
    return value.map((event, index) => normalizeEvent(event, `${field}[${index}]`));
}

function normalizeEvent(value: unknown, field: string): AuthoringHostEvent {
    const record = requireRecord(value, field);
    return {
        type: requireEventType(record.type, `${field}.type`),
        entity: nullableString(record.entity, `${field}.entity`),
        path: nullableString(record.path, `${field}.path`),
        message: requireString(record.message, `${field}.message`),
    };
}

function requireEventType(value: unknown, field: string): AuthoringHostEventType {
    const text = requireString(value, field);
    switch (text) {
        case "sceneReset":
        case "sceneCreated":
        case "sceneLoaded":
        case "sceneSaved":
        case "sceneDirtyChanged":
        case "sceneSettingsChanged":
        case "historyChanged":
        case "entityCreated":
        case "entityModified":
        case "entityDestroyed":
        case "entityReparented":
        case "componentAdded":
        case "componentRemoved":
        case "unknown":
            return text;
        default:
            throw new Error(`Field '${field}' has unsupported authoring host event type '${text}'.`);
    }
}
