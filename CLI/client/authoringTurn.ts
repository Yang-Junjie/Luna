import {
    type AuthoringCapabilitiesDocument,
    type AuthoringPlan,
    type AuthoringReport,
    type AuthoringRepairContext,
} from "./authoringProtocol.ts";
import { AuthoringSessionController } from "./authoringController.ts";
import { type AuthoringHostEvent, type AuthoringHostSessionState } from "./authoringHostClient.ts";

export type AuthoringTurnPlannerContext = {
    intent: string;
    project?: string;
    attempt: number;
    maxAttempts: number;
    capabilities: AuthoringCapabilitiesDocument;
    session: AuthoringHostSessionState;
    conversation?: AuthoringConversationMessage[];
    previousAttempt?: AuthoringTurnAttempt;
};

export type AuthoringTurnPlanner = (
    context: AuthoringTurnPlannerContext,
) => Promise<AuthoringPlan> | AuthoringPlan;

export type AuthoringConversationMessage = {
    role: "system" | "user" | "assistant";
    content: string;
};

export type AuthoringTurnAttempt = {
    attempt: number;
    transactionName: string;
    sessionBefore: AuthoringHostSessionState;
    sessionAfter: AuthoringHostSessionState;
    plan: AuthoringPlan;
    ok: boolean;
    report: AuthoringReport;
    repair: AuthoringRepairContext;
    events: AuthoringHostEvent[];
    committed: boolean;
};

export type AuthoringTurnResult = {
    ok: boolean;
    committed: boolean;
    intent: string;
    project?: string;
    transactionName: string;
    maxAttempts: number;
    capabilities: AuthoringCapabilitiesDocument;
    sessionBefore: AuthoringHostSessionState;
    sessionAfter: AuthoringHostSessionState;
    attempts: AuthoringTurnAttempt[];
    report: AuthoringReport;
    repair: AuthoringRepairContext;
    events: AuthoringHostEvent[];
};

export type AuthoringTurnOptions = {
    project?: string;
    transactionName?: string;
    maxAttempts?: number;
    commitOnSuccess?: boolean;
    conversation?: AuthoringConversationMessage[];
};

export function buildAuthoringTurnTransactionName(intent: string): string {
    const normalized = intent.trim().replace(/\s+/g, " ");
    return normalized.length === 0 ? "Authoring Turn" : `Turn: ${normalized.slice(0, 64)}`;
}

export class AuthoringTurn {
    #controller: AuthoringSessionController;
    #defaultTransactionName: string;

    constructor(controller: AuthoringSessionController, options: Pick<AuthoringTurnOptions, "transactionName"> = {}) {
        this.#controller = controller;
        this.#defaultTransactionName = options.transactionName ?? "Authoring Turn";
    }

    async run(
        intent: string,
        planner: AuthoringTurnPlanner,
        options: AuthoringTurnOptions = {},
    ): Promise<AuthoringTurnResult> {
        const state = await this.#controller.state();
        if (state.session.hasOpenTransaction) {
            throw new Error("Authoring turn requires a closed session.");
        }

        const maxAttempts = Math.max(1, Math.trunc(options.maxAttempts ?? 1));
        const transactionName = options.transactionName ?? this.#defaultTransactionName;
        const commitOnSuccess = options.commitOnSuccess ?? true;
        const attempts: AuthoringTurnAttempt[] = [];
        let finalSession = state.session;

        for (let attempt = 1; attempt <= maxAttempts; ++attempt) {
            const beginResult = await this.#controller.beginTransaction(transactionName);
            if (!beginResult.ok) {
                throw new Error(`Could not open authoring transaction '${transactionName}'.`);
            }

            const attemptContext: AuthoringTurnPlannerContext = {
                intent,
            project: options.project,
            attempt,
            maxAttempts,
            capabilities: state.capabilities,
            session: beginResult.session,
            conversation: options.conversation,
            previousAttempt: attempts.at(-1),
        };

            let plan: AuthoringPlan;
            let execution: Awaited<ReturnType<AuthoringSessionController["executePlan"]>>;
            try {
                plan = await planner(attemptContext);
                execution = await this.#controller.executePlan(plan);
            } catch (error) {
                try {
                    const rollback = await this.#controller.rollbackTransaction();
                    finalSession = rollback.session;
                } catch {
                    // Best-effort cleanup only.
                }
                throw error;
            }

            const attemptRecord: AuthoringTurnAttempt = {
                attempt,
                transactionName,
                sessionBefore: beginResult.session,
                sessionAfter: execution.session,
                plan,
                ok: execution.ok,
                report: execution.report,
                repair: execution.repair,
                events: [...execution.events],
                committed: false,
            };

            if (execution.ok) {
                if (commitOnSuccess) {
                    const commit = await this.#controller.commitTransaction();
                    attemptRecord.events = [...attemptRecord.events, ...commit.events];
                    if (commit.ok) {
                        attemptRecord.sessionAfter = commit.session;
                        attemptRecord.committed = true;
                        attempts.push(attemptRecord);
                        finalSession = commit.session;
                        return finalizeTurnResult({
                            ok: true,
                            committed: true,
                            intent,
                            project: options.project,
                            transactionName,
                            maxAttempts,
                            capabilities: state.capabilities,
                            sessionBefore: state.session,
                            sessionAfter: finalSession,
                            attempts,
                            report: execution.report,
                            repair: execution.repair,
                            events: attemptRecord.events,
                        });
                    }

                    const recovery = await this.#controller.rollbackTransaction().catch(() => undefined);
                    if (recovery != null) {
                        attemptRecord.sessionAfter = recovery.session;
                        attemptRecord.events = [...attemptRecord.events, ...recovery.events];
                        finalSession = recovery.session;
                    }
                    attempts.push(attemptRecord);
                    return finalizeTurnResult({
                        ok: false,
                        committed: false,
                        intent,
                        project: options.project,
                        transactionName,
                        maxAttempts,
                        capabilities: state.capabilities,
                        sessionBefore: state.session,
                        sessionAfter: finalSession,
                        attempts,
                        report: execution.report,
                        repair: execution.repair,
                        events: attemptRecord.events,
                    });
                }

                const rollback = await this.#controller.rollbackTransaction();
                attemptRecord.sessionAfter = rollback.session;
                attemptRecord.events = [...attemptRecord.events, ...rollback.events];
                attempts.push(attemptRecord);
                finalSession = rollback.session;
                return finalizeTurnResult({
                    ok: execution.ok,
                    committed: false,
                    intent,
                    project: options.project,
                    transactionName,
                    maxAttempts,
                    capabilities: state.capabilities,
                    sessionBefore: state.session,
                    sessionAfter: finalSession,
                    attempts,
                    report: execution.report,
                    repair: execution.repair,
                    events: attemptRecord.events,
                });
            }

            const shouldRetry = attempt < maxAttempts && execution.repair.retryable;
            const rollback = await this.#controller.rollbackTransaction();
            attemptRecord.sessionAfter = rollback.session;
            attemptRecord.events = [...attemptRecord.events, ...rollback.events];
            attempts.push(attemptRecord);
            finalSession = rollback.session;
            if (shouldRetry) {
                continue;
            }

            return finalizeTurnResult({
                ok: false,
                committed: false,
                intent,
                project: options.project,
                transactionName,
                maxAttempts,
                capabilities: state.capabilities,
                sessionBefore: state.session,
                sessionAfter: finalSession,
                attempts,
                report: execution.report,
                repair: execution.repair,
                events: attemptRecord.events,
            });
        }

        const fallbackAttempt = attempts.at(-1);
        if (fallbackAttempt == null) {
            throw new Error("Authoring turn did not produce an attempt.");
        }

        return finalizeTurnResult({
            ok: fallbackAttempt.ok,
            committed: fallbackAttempt.committed,
            intent,
            project: options.project,
            transactionName,
            maxAttempts,
            capabilities: state.capabilities,
            sessionBefore: state.session,
            sessionAfter: finalSession,
            attempts,
            report: fallbackAttempt.report,
            repair: fallbackAttempt.repair,
            events: fallbackAttempt.events,
        });
    }
}

type TurnResultInit = {
    ok: boolean;
    committed: boolean;
    intent: string;
    project?: string;
    transactionName: string;
    maxAttempts: number;
    capabilities: AuthoringCapabilitiesDocument;
    sessionBefore: AuthoringHostSessionState;
    sessionAfter: AuthoringHostSessionState;
    attempts: AuthoringTurnAttempt[];
    report: AuthoringReport;
    repair: AuthoringRepairContext;
    events: AuthoringHostEvent[];
};

function finalizeTurnResult(init: TurnResultInit): AuthoringTurnResult {
    return {
        ...init,
        events: [...init.events],
        attempts: init.attempts.map((attempt) => ({
            ...attempt,
            events: [...attempt.events],
        })),
        report: init.report,
        repair: init.repair,
    };
}
