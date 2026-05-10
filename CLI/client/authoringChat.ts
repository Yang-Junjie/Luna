import { type AuthoringHostSessionState } from "./authoringHostClient.ts";
import {
    type AuthoringConversationMessage,
    type AuthoringTurnResult,
} from "./authoringTurn.ts";

export type AuthoringChatTranscriptOptions = {
    limit?: number;
};

export class AuthoringChatTranscript {
    #messages: AuthoringConversationMessage[] = [];
    #limit: number;

    constructor(options: AuthoringChatTranscriptOptions = {}) {
        this.#limit = Math.max(0, Math.trunc(options.limit ?? 12));
    }

    messages(): AuthoringConversationMessage[] {
        return [...this.#messages];
    }

    clear(): void {
        this.#messages = [];
    }

    appendUser(content: string): void {
        this.#append("user", content);
    }

    appendAssistant(content: string): void {
        this.#append("assistant", content);
    }

    appendSystem(content: string): void {
        this.#append("system", content);
    }

    #append(role: AuthoringConversationMessage["role"], content: string): void {
        if (this.#limit === 0) {
            return;
        }

        this.#messages.push({ role, content });
        const overflow = this.#messages.length - this.#limit;
        if (overflow > 0) {
            this.#messages.splice(0, overflow);
        }
    }
}

export function renderAuthoringChatTurnSummary(result: AuthoringTurnResult): string {
    const attempt = result.attempts.at(-1);
    const plan = attempt?.plan;
    const summary = {
        intent: result.intent,
        ok: result.ok,
        committed: result.committed,
        attempts: result.attempts.length,
        transactionName: result.transactionName,
        scene: {
            name: result.report.scene.name,
            path: result.report.scene.path,
            entityCount: result.report.scene.entityCount,
            dirty: result.report.scene.dirty,
        },
        session: {
            hasOpenTransaction: result.sessionAfter.hasOpenTransaction,
            canUndo: result.sessionAfter.canUndo,
            canRedo: result.sessionAfter.canRedo,
            undoDepth: result.sessionAfter.undoDepth,
            redoDepth: result.sessionAfter.redoDepth,
        },
        plan: plan == null ? null : {
            commandCount: plan.commands.length,
            ops: plan.commands.map((command) => command.op),
        },
        repair: {
            ok: result.repair.ok,
            retryable: result.repair.retryable,
            diagnostics: result.repair.diagnostics.slice(0, 3).map((diagnostic) => ({
                code: diagnostic.code,
                message: diagnostic.message,
                field: diagnostic.field,
            })),
        },
    };

    return JSON.stringify(summary);
}

export function renderAuthoringSessionState(session: AuthoringHostSessionState): string {
    return JSON.stringify({
        hasScene: session.hasScene,
        hasOpenTransaction: session.hasOpenTransaction,
        canUndo: session.canUndo,
        canRedo: session.canRedo,
        undoDepth: session.undoDepth,
        redoDepth: session.redoDepth,
        scene: {
            name: session.scene.name,
            path: session.scene.path,
            entityCount: session.scene.entityCount,
            dirty: session.scene.dirty,
        },
    });
}
