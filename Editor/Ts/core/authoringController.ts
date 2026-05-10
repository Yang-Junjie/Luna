import {
    buildAuthoringRepairContext,
    type AuthoringCapabilitiesDocument,
    type AuthoringPlan,
    type AuthoringReport,
    type AuthoringRepairContext,
} from "./authoringProtocol.ts";
import {
    AuthoringHostClient,
    type AuthoringHostEvent,
    type AuthoringHostMutationResult,
    type AuthoringHostSessionResult,
    type AuthoringHostSessionState,
} from "./authoringHostClient.ts";

export type AuthoringSessionControllerState = {
    capabilities: AuthoringCapabilitiesDocument;
    session: AuthoringHostSessionState;
};

export type AuthoringSessionControllerRunResult = {
    ok: boolean;
    openedTransaction: boolean;
    sessionBefore: AuthoringHostSessionState;
    sessionAfter: AuthoringHostSessionState;
    report: AuthoringReport;
    repair: AuthoringRepairContext;
    events: AuthoringHostEvent[];
};

export type AuthoringSessionControllerExecutionResult = {
    ok: boolean;
    session: AuthoringHostSessionState;
    report: AuthoringReport;
    repair: AuthoringRepairContext;
    events: AuthoringHostEvent[];
};

export type AuthoringSessionControllerMutationState = AuthoringHostMutationResult & {
    session: AuthoringHostSessionState;
};

export type AuthoringSessionControllerOptions = {
    defaultTransactionName?: string;
};

export class AuthoringSessionController {
    #client: AuthoringHostClient;
    #defaultTransactionName: string;
    #capabilities: AuthoringCapabilitiesDocument | undefined;
    #session: AuthoringHostSessionState | undefined;
    #lastReport: AuthoringReport | undefined;
    #lastRepair: AuthoringRepairContext | undefined;
    #lastEvents: AuthoringHostEvent[] = [];

    constructor(client: AuthoringHostClient, options: AuthoringSessionControllerOptions = {}) {
        this.#client = client;
        this.#defaultTransactionName = options.defaultTransactionName ?? "Authoring Session";
    }

    get defaultTransactionName(): string {
        return this.#defaultTransactionName;
    }

    lastReport(): AuthoringReport | undefined {
        return this.#lastReport;
    }

    lastRepair(): AuthoringRepairContext | undefined {
        return this.#lastRepair;
    }

    lastEvents(): AuthoringHostEvent[] {
        return [...this.#lastEvents];
    }

    async capabilities(): Promise<AuthoringCapabilitiesDocument> {
        if (this.#capabilities == null) {
            this.#capabilities = await this.#client.capabilities();
        }
        return this.#capabilities;
    }

    async session(): Promise<AuthoringHostSessionState> {
        return await this.#refreshSession();
    }

    async state(): Promise<AuthoringSessionControllerState> {
        const [capabilities, session] = await Promise.all([this.capabilities(), this.session()]);
        return { capabilities, session };
    }

    async beginTransaction(name = this.#defaultTransactionName): Promise<AuthoringHostSessionResult> {
        const result = await this.#client.beginTransaction(name);
        this.#session = result.session;
        return result;
    }

    async commitTransaction(): Promise<AuthoringSessionControllerMutationState> {
        const result = await this.#client.commitTransaction();
        return await this.#refreshMutationState(result);
    }

    async rollbackTransaction(): Promise<AuthoringSessionControllerMutationState> {
        const result = await this.#client.rollbackTransaction();
        return await this.#refreshMutationState(result);
    }

    async clearHistory(): Promise<AuthoringHostSessionResult> {
        const result = await this.#client.clearHistory();
        this.#session = result.session;
        return result;
    }

    async executePlan(plan: AuthoringPlan): Promise<AuthoringSessionControllerExecutionResult> {
        const result = await this.#client.executePlan(plan);
        return await this.#refreshExecutionState(result);
    }

    async snapshot(): Promise<AuthoringSessionControllerExecutionResult> {
        const result = await this.#client.snapshot();
        return await this.#refreshExecutionState(result);
    }

    async undo(): Promise<AuthoringSessionControllerMutationState> {
        const result = await this.#client.undo();
        return await this.#refreshMutationState(result);
    }

    async redo(): Promise<AuthoringSessionControllerMutationState> {
        const result = await this.#client.redo();
        return await this.#refreshMutationState(result);
    }

    async clearAliases(): Promise<boolean> {
        return await this.#client.clearAliases();
    }

    async runPlan(
        plan: AuthoringPlan,
        transactionName = this.#defaultTransactionName,
    ): Promise<AuthoringSessionControllerRunResult> {
        const sessionBefore = await this.session();
        let openedTransaction = false;
        if (!sessionBefore.hasOpenTransaction) {
            const beginResult = await this.beginTransaction(transactionName);
            if (!beginResult.ok) {
                throw new Error(`Could not open authoring transaction '${transactionName}'.`);
            }
            openedTransaction = true;
        }

        const execution = await this.executePlan(plan);
        const sessionAfter = this.#session ?? await this.#refreshSession();
        return {
            ok: execution.ok,
            openedTransaction,
            sessionBefore,
            sessionAfter,
            report: execution.report,
            repair: execution.repair,
            events: execution.events,
        };
    }

    async #refreshSession(): Promise<AuthoringHostSessionState> {
        this.#session = await this.#client.session();
        return this.#session;
    }

    async #refreshMutationState(result: AuthoringHostMutationResult): Promise<AuthoringSessionControllerMutationState> {
        this.#lastEvents = result.events;
        this.#session = await this.#client.session();
        return {
            ...result,
            session: this.#session,
        };
    }

    async #refreshExecutionState(
        result: { ok: boolean; report: AuthoringReport; events: AuthoringHostEvent[] },
    ): Promise<AuthoringSessionControllerExecutionResult> {
        this.#lastReport = result.report;
        this.#lastRepair = buildAuthoringRepairContext(result.report);
        this.#lastEvents = result.events;
        this.#session = await this.#client.session();
        return {
            ok: result.ok,
            session: this.#session,
            report: result.report,
            repair: this.#lastRepair,
            events: result.events,
        };
    }
}
