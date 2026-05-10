import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";

import { AuthoringChatTranscript, renderAuthoringChatTurnSummary } from "./authoringChat.ts";
import { AuthoringSessionController } from "./authoringController.ts";
import {
    type AuthoringHostEvent,
    type AuthoringHostSessionState,
} from "./authoringHostClient.ts";
import {
    AuthoringTurn,
    buildAuthoringTurnTransactionName,
    type AuthoringTurnPlanner,
    type AuthoringTurnResult,
} from "./authoringTurn.ts";
import {
    findEntityByName,
    findEntityByRef,
    latestHierarchyEntities,
    normalizeAuthoringPlan,
    requireFiniteNumber,
    requireNonEmptyString,
    requireVec3,
    type AuthoringEntityInspection,
    type AuthoringPlan,
    type AuthoringReport,
    type Vec3,
} from "./authoringProtocol.ts";

export type AuthoringEvalEntityExpectation = {
    ref?: string;
    name?: string;
    components?: string[];
    lightType?: string;
    cameraProjection?: string;
    translation?: Vec3;
    rotationDeg?: Vec3;
    scale?: Vec3;
    transformTolerance?: number;
};

export type AuthoringEvalExpectation = {
    ok?: boolean;
    committed?: boolean;
    noDiagnostics?: boolean;
    canUndo?: boolean;
    canRedo?: boolean;
    undoDepth?: number;
    redoDepth?: number;
    attemptCount?: number;
    failedAttemptsAtLeast?: number;
    diagnosticCodes?: string[];
    entityCountAtLeast?: number;
    componentCounts?: Record<string, number>;
    entities?: AuthoringEvalEntityExpectation[];
    planOps?: string[];
    planAnyOps?: string[][];
    planOpCounts?: Record<string, number>;
    savedSceneCountAtLeast?: number;
};

export type AuthoringEvalControl = "undo" | "redo" | "snapshot" | "clear-history";

export type AuthoringEvalTurnSpec = {
    id?: string;
    intent?: string;
    control?: AuthoringEvalControl;
    firstAttemptPlan?: AuthoringPlan;
    maxAttempts?: number;
    expect?: AuthoringEvalExpectation;
};

export type AuthoringEvalSpec = {
    name: string;
    description?: string;
    project?: string;
    execute?: boolean;
    maxAttempts?: number;
    turns: AuthoringEvalTurnSpec[];
};

export type AuthoringEvalPlannerFactory = (
    intent: string,
    turn: AuthoringEvalTurnSpec,
) => AuthoringTurnPlanner;

export type AuthoringEvalRunOptions = {
    controller: AuthoringSessionController;
    spec: AuthoringEvalSpec;
    createPlanner: AuthoringEvalPlannerFactory;
    execute?: boolean;
    maxAttempts?: number;
    project?: string;
    runDir: string;
    resultPath?: string;
};

export type AuthoringEvalTurnResult = {
    id: string;
    intent: string;
    kind: "turn" | "control";
    ok: boolean;
    failures: string[];
    error?: string;
    plan?: AuthoringPlan;
    turn?: AuthoringTurnResult;
    control?: AuthoringEvalControlResult;
    evaluationReport?: AuthoringReport;
};

export type AuthoringEvalControlResult = {
    action: AuthoringEvalControl;
    ok: boolean;
    session: AuthoringHostSessionState;
    events: AuthoringHostEvent[];
};

export type AuthoringEvalResult = {
    protocol: {
        name: "luna.authoring.eval";
        version: 1;
    };
    ok: boolean;
    name: string;
    description?: string;
    runId: string;
    runDir: string;
    resultPath?: string;
    mode: "execute" | "rollback";
    startedAt: string;
    completedAt: string;
    project?: string;
    turns: AuthoringEvalTurnResult[];
};

export function readAuthoringEvalSpec(path: string): AuthoringEvalSpec {
    return normalizeAuthoringEvalSpec(JSON.parse(readFileSync(path, "utf8")) as unknown);
}

export function createAuthoringEvalRunDirectory(outputRoot: string, evalName: string, now = new Date()): string {
    const runId = createAuthoringEvalRunId(now);
    const runDir = resolve(outputRoot, `${safeFileName(evalName)}-${runId}`);
    mkdirSync(runDir, { recursive: true });
    return runDir;
}

export function createAuthoringEvalRunId(now = new Date()): string {
    return now.toISOString().replace(/[:.]/g, "-");
}

export async function runAuthoringEval(options: AuthoringEvalRunOptions): Promise<AuthoringEvalResult> {
    const startedAt = new Date().toISOString();
    const runId = extractRunId(options.runDir);
    const execute = options.execute ?? options.spec.execute ?? true;
    const maxAttempts = options.maxAttempts ?? options.spec.maxAttempts ?? 1;
    const project = options.project ?? options.spec.project;
    const transcript = new AuthoringChatTranscript({ limit: 24 });
    const turnRunner = new AuthoringTurn(options.controller);
    const turns: AuthoringEvalTurnResult[] = [];

    for (const [index, turnSpec] of options.spec.turns.entries()) {
        const id = turnSpec.id ?? `turn-${index + 1}`;
        const intent = turnSpec.intent == null ? undefined : renderEvalText(turnSpec.intent, {
            evalName: options.spec.name,
            outputDir: toPromptPath(options.runDir),
            runDir: toPromptPath(options.runDir),
        });
        const label = intent ?? `/${turnSpec.control ?? "turn"}`;

        try {
            if (turnSpec.control != null) {
                const control = await runControlStep(options.controller, turnSpec.control);
                const evaluationReport = await readControlEvaluationReport(options.controller, control);
                const failures = evaluateControl(turnSpec.expect, control, evaluationReport);
                turns.push({
                    id,
                    intent: label,
                    kind: "control",
                    ok: control.ok && failures.length === 0,
                    failures,
                    control,
                    evaluationReport,
                });
                transcript.appendAssistant(JSON.stringify({
                    control: control.action,
                    ok: control.ok,
                    session: control.session,
                }));
                continue;
            }

            if (intent == null) {
                throw new Error(`Eval turn '${id}' is missing an intent.`);
            }

            const result = await turnRunner.run(intent, createEvalTurnPlanner(options.createPlanner, intent, turnSpec), {
                project,
                transactionName: buildAuthoringTurnTransactionName(`[eval:${options.spec.name}] ${intent}`),
                maxAttempts: turnSpec.maxAttempts ?? maxAttempts,
                commitOnSuccess: execute,
                conversation: transcript.messages(),
            });
            const evaluationReport = await readEvaluationReport(options.controller, result);
            const plan = finalTurnPlan(result);
            const failures = evaluateTurn(turnSpec.expect, result, plan, evaluationReport);
            turns.push({
                id,
                intent,
                kind: "turn",
                ok: result.ok && failures.length === 0,
                failures,
                plan,
                turn: result,
                evaluationReport,
            });

            transcript.appendUser(intent);
            transcript.appendAssistant(renderAuthoringChatTurnSummary(result));
        } catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            turns.push({
                id,
                intent: label,
                kind: turnSpec.control == null ? "turn" : "control",
                ok: false,
                failures: [message],
                error: message,
            });
            transcript.appendUser(intent);
            transcript.appendAssistant(JSON.stringify({ ok: false, error: message }));
            break;
        }
    }

    const completedAt = new Date().toISOString();
    return {
        protocol: { name: "luna.authoring.eval", version: 1 },
        ok: turns.length === options.spec.turns.length && turns.every((turn) => turn.ok),
        name: options.spec.name,
        description: options.spec.description,
        runId,
        runDir: options.runDir,
        resultPath: options.resultPath,
        mode: execute ? "execute" : "rollback",
        startedAt,
        completedAt,
        project,
        turns,
    };
}

export function writeAuthoringEvalResult(path: string, result: AuthoringEvalResult): void {
    mkdirSync(dirname(path), { recursive: true });
    writeFileSync(path, `${JSON.stringify(result, null, 2)}\n`, "utf8");
}

export function renderAuthoringEvalSummary(result: AuthoringEvalResult): string {
    const lines = [
        `Authoring eval '${result.name}': ${result.ok ? "PASS" : "FAIL"}`,
        `Mode: ${result.mode}`,
        `Run: ${result.runId}`,
    ];
    if (result.resultPath != null) {
        lines.push(`Result: ${result.resultPath}`);
    }
    for (const turn of result.turns) {
        lines.push(`- ${turn.id}: ${turn.ok ? "PASS" : "FAIL"} (${turn.intent})`);
        for (const failure of turn.failures) {
            lines.push(`  ${failure}`);
        }
    }
    return `${lines.join("\n")}\n`;
}

function normalizeAuthoringEvalSpec(input: unknown): AuthoringEvalSpec {
    const record = requireRecord(input, "eval");
    const name = requireNonEmptyString(record.name, "name");
    const turns = requireArray(record.turns, "turns").map((turn, index) => normalizeTurnSpec(turn, index));
    if (turns.length === 0) {
        throw new Error("Authoring eval must contain at least one turn.");
    }

    return {
        name,
        description: optionalString(record.description, "description"),
        project: optionalString(record.project, "project"),
        execute: optionalBoolean(record.execute, "execute"),
        maxAttempts: optionalPositiveInteger(record.maxAttempts, "maxAttempts"),
        turns,
    };
}

function normalizeTurnSpec(input: unknown, index: number): AuthoringEvalTurnSpec {
    const record = requireRecord(input, `turns[${index}]`);
    const intent = optionalString(record.intent, `turns[${index}].intent`);
    const control = optionalControl(record.control, `turns[${index}].control`);
    if (intent == null && control == null) {
        throw new Error(`Eval turn 'turns[${index}]' must include an intent or control.`);
    }

    return {
        id: optionalString(record.id, `turns[${index}].id`),
        intent,
        control,
        firstAttemptPlan: record.firstAttemptPlan == null
            ? undefined
            : normalizeAuthoringPlan(record.firstAttemptPlan),
        maxAttempts: optionalPositiveInteger(record.maxAttempts, `turns[${index}].maxAttempts`),
        expect: record.expect == null ? undefined : normalizeExpectation(record.expect, `turns[${index}].expect`),
    };
}

function createEvalTurnPlanner(
    createPlanner: AuthoringEvalPlannerFactory,
    intent: string,
    turnSpec: AuthoringEvalTurnSpec,
): AuthoringTurnPlanner {
    const planner = createPlanner(intent, turnSpec);
    if (turnSpec.firstAttemptPlan == null) {
        return planner;
    }

    return (context) => context.attempt === 1
        ? turnSpec.firstAttemptPlan as AuthoringPlan
        : planner(context);
}

function normalizeExpectation(input: unknown, field: string): AuthoringEvalExpectation {
    const record = requireRecord(input, field);
    return {
        ok: optionalBoolean(record.ok, `${field}.ok`),
        committed: optionalBoolean(record.committed, `${field}.committed`),
        noDiagnostics: optionalBoolean(record.noDiagnostics, `${field}.noDiagnostics`),
        canUndo: optionalBoolean(record.canUndo, `${field}.canUndo`),
        canRedo: optionalBoolean(record.canRedo, `${field}.canRedo`),
        undoDepth: optionalNonNegativeInteger(record.undoDepth, `${field}.undoDepth`),
        redoDepth: optionalNonNegativeInteger(record.redoDepth, `${field}.redoDepth`),
        attemptCount: optionalNonNegativeInteger(record.attemptCount, `${field}.attemptCount`),
        failedAttemptsAtLeast: optionalNonNegativeInteger(
            record.failedAttemptsAtLeast,
            `${field}.failedAttemptsAtLeast`,
        ),
        diagnosticCodes: record.diagnosticCodes == null
            ? undefined
            : requireStringArray(record.diagnosticCodes, `${field}.diagnosticCodes`),
        entityCountAtLeast: optionalNonNegativeInteger(record.entityCountAtLeast, `${field}.entityCountAtLeast`),
        componentCounts: optionalStringIntegerMap(record.componentCounts, `${field}.componentCounts`),
        entities: record.entities == null
            ? undefined
            : requireArray(record.entities, `${field}.entities`)
                .map((entity, index) => normalizeEntityExpectation(entity, `${field}.entities[${index}]`)),
        planOps: record.planOps == null ? undefined : requireStringArray(record.planOps, `${field}.planOps`),
        planAnyOps: record.planAnyOps == null
            ? undefined
            : requireArray(record.planAnyOps, `${field}.planAnyOps`).map((group, index) =>
                requireStringArray(group, `${field}.planAnyOps[${index}]`)),
        planOpCounts: optionalStringIntegerMap(record.planOpCounts, `${field}.planOpCounts`),
        savedSceneCountAtLeast: optionalNonNegativeInteger(
            record.savedSceneCountAtLeast,
            `${field}.savedSceneCountAtLeast`,
        ),
    };
}

function normalizeEntityExpectation(input: unknown, field: string): AuthoringEvalEntityExpectation {
    const record = requireRecord(input, field);
    return {
        ref: optionalString(record.ref, `${field}.ref`),
        name: optionalString(record.name, `${field}.name`),
        components: record.components == null ? undefined : requireStringArray(record.components, `${field}.components`),
        lightType: optionalString(record.lightType, `${field}.lightType`),
        cameraProjection: optionalString(record.cameraProjection, `${field}.cameraProjection`),
        translation: record.translation == null ? undefined : requireVec3(record.translation, `${field}.translation`),
        rotationDeg: record.rotationDeg == null ? undefined : requireVec3(record.rotationDeg, `${field}.rotationDeg`),
        scale: record.scale == null ? undefined : requireVec3(record.scale, `${field}.scale`),
        transformTolerance: optionalNonNegativeNumber(record.transformTolerance, `${field}.transformTolerance`),
    };
}

async function runControlStep(
    controller: AuthoringSessionController,
    action: AuthoringEvalControl,
): Promise<AuthoringEvalControlResult> {
    if (action === "undo") {
        const result = await controller.undo();
        return { action, ok: result.ok, session: result.session, events: result.events };
    }
    if (action === "redo") {
        const result = await controller.redo();
        return { action, ok: result.ok, session: result.session, events: result.events };
    }
    if (action === "clear-history") {
        const result = await controller.clearHistory();
        return { action, ok: result.ok, session: result.session, events: [] };
    }

    const session = await controller.session();
    return { action, ok: true, session, events: [] };
}

async function readEvaluationReport(
    controller: AuthoringSessionController,
    result: AuthoringTurnResult,
): Promise<AuthoringReport> {
    if (!result.committed) {
        return result.report;
    }

    const snapshot = await controller.snapshot();
    return snapshot.report;
}

async function readControlEvaluationReport(
    controller: AuthoringSessionController,
    control: AuthoringEvalControlResult,
): Promise<AuthoringReport> {
    if (!control.session.hasScene) {
        throw new Error(`Control step '${control.action}' left no scene to snapshot.`);
    }

    const snapshot = await controller.snapshot();
    return snapshot.report;
}

function finalTurnPlan(result: AuthoringTurnResult): AuthoringPlan | undefined {
    return result.attempts.at(-1)?.plan;
}

function evaluateTurn(
    expect: AuthoringEvalExpectation | undefined,
    result: AuthoringTurnResult,
    plan: AuthoringPlan | undefined,
    evaluationReport: AuthoringReport,
): string[] {
    const failures: string[] = [];
    const expected = expect ?? {};
    const expectedOk = expected.ok ?? true;
    if (result.ok !== expectedOk) {
        failures.push(`Expected turn ok=${expectedOk}, got ${result.ok}.`);
    }
    if (expected.committed != null && result.committed !== expected.committed) {
        failures.push(`Expected committed=${expected.committed}, got ${result.committed}.`);
    }
    if (expected.attemptCount != null && result.attempts.length !== expected.attemptCount) {
        failures.push(`Expected ${expected.attemptCount} attempts, got ${result.attempts.length}.`);
    }
    if (expected.failedAttemptsAtLeast != null) {
        const failedAttempts = result.attempts.filter((attempt) => !attempt.ok).length;
        if (failedAttempts < expected.failedAttemptsAtLeast) {
            failures.push(`Expected at least ${expected.failedAttemptsAtLeast} failed attempts, got ${failedAttempts}.`);
        }
    }

    const noDiagnostics = expected.noDiagnostics ?? expectedOk;
    if (noDiagnostics) {
        const diagnostics = [...result.report.diagnostics, ...evaluationReport.diagnostics];
        if (diagnostics.length > 0) {
            failures.push(`Expected no diagnostics, got ${diagnostics.length}.`);
        }
    }
    evaluateDiagnosticCodes(expected, result, failures);
    evaluateSession(expected, result.sessionAfter, failures);

    if (expected.entityCountAtLeast != null && evaluationReport.scene.entityCount < expected.entityCountAtLeast) {
        failures.push(
            `Expected at least ${expected.entityCountAtLeast} entities, got ${evaluationReport.scene.entityCount}.`,
        );
    }
    if (expected.savedSceneCountAtLeast != null && result.report.savedScenes.length < expected.savedSceneCountAtLeast) {
        failures.push(
            `Expected at least ${expected.savedSceneCountAtLeast} saved scenes, got ${result.report.savedScenes.length}.`,
        );
    }

    evaluatePlanOps(expected, plan, failures);
    evaluateComponentCounts(expected, evaluationReport, failures);
    evaluateEntities(expected, evaluationReport, failures);
    return failures;
}

function evaluateControl(
    expect: AuthoringEvalExpectation | undefined,
    control: AuthoringEvalControlResult,
    evaluationReport: AuthoringReport,
): string[] {
    const failures: string[] = [];
    const expected = expect ?? {};
    const expectedOk = expected.ok ?? true;
    if (control.ok !== expectedOk) {
        failures.push(`Expected control ok=${expectedOk}, got ${control.ok}.`);
    }

    const noDiagnostics = expected.noDiagnostics ?? expectedOk;
    if (noDiagnostics && evaluationReport.diagnostics.length > 0) {
        failures.push(`Expected no diagnostics, got ${evaluationReport.diagnostics.length}.`);
    }
    evaluateSession(expected, control.session, failures);

    if (expected.entityCountAtLeast != null && evaluationReport.scene.entityCount < expected.entityCountAtLeast) {
        failures.push(
            `Expected at least ${expected.entityCountAtLeast} entities, got ${evaluationReport.scene.entityCount}.`,
        );
    }

    evaluateComponentCounts(expected, evaluationReport, failures);
    evaluateEntities(expected, evaluationReport, failures);
    return failures;
}

function evaluateDiagnosticCodes(
    expected: AuthoringEvalExpectation,
    result: AuthoringTurnResult,
    failures: string[],
): void {
    if (expected.diagnosticCodes == null) {
        return;
    }

    const actualCodes = new Set(
        result.attempts.flatMap((attempt) => attempt.report.diagnostics.map((diagnostic) => diagnostic.code)),
    );
    for (const code of expected.diagnosticCodes) {
        if (!actualCodes.has(code)) {
            failures.push(`Expected diagnostic code '${code}', but got [${[...actualCodes].join(", ")}].`);
        }
    }
}

function evaluateSession(
    expected: AuthoringEvalExpectation,
    session: AuthoringHostSessionState,
    failures: string[],
): void {
    if (expected.canUndo != null && session.canUndo !== expected.canUndo) {
        failures.push(`Expected canUndo=${expected.canUndo}, got ${session.canUndo}.`);
    }
    if (expected.canRedo != null && session.canRedo !== expected.canRedo) {
        failures.push(`Expected canRedo=${expected.canRedo}, got ${session.canRedo}.`);
    }
    if (expected.undoDepth != null && session.undoDepth !== expected.undoDepth) {
        failures.push(`Expected undoDepth=${expected.undoDepth}, got ${session.undoDepth}.`);
    }
    if (expected.redoDepth != null && session.redoDepth !== expected.redoDepth) {
        failures.push(`Expected redoDepth=${expected.redoDepth}, got ${session.redoDepth}.`);
    }
}

function evaluatePlanOps(
    expected: AuthoringEvalExpectation,
    plan: AuthoringPlan | undefined,
    failures: string[],
): void {
    if (expected.planOps == null && expected.planOpCounts == null && expected.planAnyOps == null) {
        return;
    }
    if (plan == null) {
        failures.push("Expected generated plan, got none.");
        return;
    }

    const ops = plan.commands.map((command) => command.op);
    for (const op of expected.planOps ?? []) {
        if (!ops.includes(op)) {
            failures.push(`Expected plan op '${op}', but generated ops were [${ops.join(", ")}].`);
        }
    }
    for (const group of expected.planAnyOps ?? []) {
        if (!group.some((op) => ops.includes(op))) {
            failures.push(`Expected at least one of [${group.join(", ")}], but generated ops were [${ops.join(", ")}].`);
        }
    }
    for (const [op, minimum] of Object.entries(expected.planOpCounts ?? {})) {
        const actual = ops.filter((candidate) => candidate === op).length;
        if (actual < minimum) {
            failures.push(`Expected at least ${minimum} '${op}' plan ops, got ${actual}.`);
        }
    }
}

function evaluateComponentCounts(
    expected: AuthoringEvalExpectation,
    report: AuthoringReport,
    failures: string[],
): void {
    if (expected.componentCounts == null) {
        return;
    }

    const entities = latestHierarchyEntities(report);
    if (entities.length === 0) {
        failures.push("Expected hierarchy snapshot for component checks, got none.");
        return;
    }

    for (const [component, minimum] of Object.entries(expected.componentCounts)) {
        const actual = entities.filter((entity) => entity.components.includes(component)).length;
        if (actual < minimum) {
            failures.push(`Expected at least ${minimum} entities with '${component}', got ${actual}.`);
        }
    }
}

function evaluateEntities(
    expected: AuthoringEvalExpectation,
    report: AuthoringReport,
    failures: string[],
): void {
    for (const entityExpectation of expected.entities ?? []) {
        const entity = findExpectedEntity(report, entityExpectation);
        const label = entityExpectation.ref ?? entityExpectation.name ?? "<unnamed>";
        if (entity == null) {
            failures.push(`Expected entity '${label}', but it was not found.`);
            continue;
        }

        for (const component of entityExpectation.components ?? []) {
            if (!entity.components.includes(component)) {
                failures.push(`Expected entity '${label}' to have component '${component}'.`);
            }
        }
        if (entityExpectation.lightType != null && entity.light?.type !== entityExpectation.lightType) {
            failures.push(`Expected entity '${label}' light type '${entityExpectation.lightType}', got '${entity.light?.type ?? ""}'.`);
        }
        if (entityExpectation.cameraProjection != null && entity.camera?.projection !== entityExpectation.cameraProjection) {
            failures.push(
                `Expected entity '${label}' camera projection '${entityExpectation.cameraProjection}', ` +
                `got '${entity.camera?.projection ?? ""}'.`,
            );
        }
        evaluateEntityTransform(entityExpectation, entity, label, failures);
    }
}

function evaluateEntityTransform(
    expectation: AuthoringEvalEntityExpectation,
    entity: AuthoringEntityInspection,
    label: string,
    failures: string[],
): void {
    const tolerance = expectation.transformTolerance ?? 0.001;
    if (expectation.translation != null) {
        if (entity.transform == null || !vec3Close(entity.transform.translation, expectation.translation, tolerance)) {
            failures.push(
                `Expected entity '${label}' translation ${formatVec3(expectation.translation)}, ` +
                `got ${formatVec3(entity.transform?.translation)}.`,
            );
        }
    }
    if (expectation.rotationDeg != null) {
        if (entity.transform == null || !vec3Close(entity.transform.rotationDeg, expectation.rotationDeg, tolerance)) {
            failures.push(
                `Expected entity '${label}' rotation ${formatVec3(expectation.rotationDeg)}, ` +
                `got ${formatVec3(entity.transform?.rotationDeg)}.`,
            );
        }
    }
    if (expectation.scale != null) {
        if (entity.transform == null || !vec3Close(entity.transform.scale, expectation.scale, tolerance)) {
            failures.push(
                `Expected entity '${label}' scale ${formatVec3(expectation.scale)}, ` +
                `got ${formatVec3(entity.transform?.scale)}.`,
            );
        }
    }
}

function vec3Close(actual: Vec3, expected: Vec3, tolerance: number): boolean {
    return actual.every((value, index) => Math.abs(value - expected[index]) <= tolerance);
}

function formatVec3(value: Vec3 | undefined): string {
    return value == null ? "<missing>" : `[${value.join(", ")}]`;
}

function findExpectedEntity(
    report: AuthoringReport,
    expectation: AuthoringEvalEntityExpectation,
): AuthoringEntityInspection | undefined {
    if (expectation.ref != null) {
        return findEntityByRef(report, expectation.ref);
    }
    if (expectation.name != null) {
        return findEntityByName(report, expectation.name);
    }
    return undefined;
}

function renderEvalText(text: string, values: Record<string, string>): string {
    return text.replace(/\{\{\s*([A-Za-z0-9_]+)\s*\}\}/g, (match, key: string) => values[key] ?? match);
}

function extractRunId(runDir: string): string {
    const normalized = runDir.replace(/\\/g, "/");
    return normalized.slice(normalized.lastIndexOf("/") + 1);
}

function toPromptPath(path: string): string {
    return resolve(path).replace(/\\/g, "/");
}

function safeFileName(value: string): string {
    const normalized = value.trim().replace(/[^A-Za-z0-9_.-]+/g, "-").replace(/^-+|-+$/g, "");
    return normalized.length === 0 ? "authoring-eval" : normalized;
}

function requireRecord(value: unknown, field: string): Record<string, unknown> {
    if (typeof value !== "object" || value == null || Array.isArray(value)) {
        throw new Error(`Field '${field}' must be an object.`);
    }
    return value as Record<string, unknown>;
}

function requireArray(value: unknown, field: string): unknown[] {
    if (!Array.isArray(value)) {
        throw new Error(`Field '${field}' must be an array.`);
    }
    return value;
}

function optionalString(value: unknown, field: string): string | undefined {
    if (value == null) {
        return undefined;
    }
    return requireNonEmptyString(value, field);
}

function optionalBoolean(value: unknown, field: string): boolean | undefined {
    if (value == null) {
        return undefined;
    }
    if (typeof value !== "boolean") {
        throw new Error(`Field '${field}' must be a boolean.`);
    }
    return value;
}

function optionalControl(value: unknown, field: string): AuthoringEvalControl | undefined {
    if (value == null) {
        return undefined;
    }
    if (value === "undo" || value === "redo" || value === "snapshot" || value === "clear-history") {
        return value;
    }
    throw new Error(`Field '${field}' must be one of undo, redo, snapshot, or clear-history.`);
}

function optionalNonNegativeNumber(value: unknown, field: string): number | undefined {
    if (value == null) {
        return undefined;
    }
    const number = requireFiniteNumber(value, field);
    if (number < 0) {
        throw new Error(`Field '${field}' must be a non-negative number.`);
    }
    return number;
}

function optionalNonNegativeInteger(value: unknown, field: string): number | undefined {
    if (value == null) {
        return undefined;
    }
    const number = requireFiniteNumber(value, field);
    if (!Number.isInteger(number) || number < 0) {
        throw new Error(`Field '${field}' must be a non-negative integer.`);
    }
    return number;
}

function optionalPositiveInteger(value: unknown, field: string): number | undefined {
    if (value == null) {
        return undefined;
    }
    const number = requireFiniteNumber(value, field);
    if (!Number.isInteger(number) || number < 1) {
        throw new Error(`Field '${field}' must be a positive integer.`);
    }
    return number;
}

function requireStringArray(value: unknown, field: string): string[] {
    return requireArray(value, field).map((item, index) => {
        if (typeof item !== "string" || item.length === 0) {
            throw new Error(`Field '${field}[${index}]' must be a non-empty string.`);
        }
        return item;
    });
}

function optionalStringIntegerMap(value: unknown, field: string): Record<string, number> | undefined {
    if (value == null) {
        return undefined;
    }

    const record = requireRecord(value, field);
    const result: Record<string, number> = {};
    for (const [key, entry] of Object.entries(record)) {
        if (key.length === 0) {
            throw new Error(`Field '${field}' cannot contain an empty key.`);
        }
        const number = requireFiniteNumber(entry, `${field}.${key}`);
        if (!Number.isInteger(number) || number < 0) {
            throw new Error(`Field '${field}.${key}' must be a non-negative integer.`);
        }
        result[key] = number;
    }
    return result;
}
