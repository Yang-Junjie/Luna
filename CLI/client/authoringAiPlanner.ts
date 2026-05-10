import {
    normalizeAuthoringPlan,
    writeAuthoringPlanJson,
    type AuthoringPlan,
} from "./authoringProtocol.ts";
import {
    type AuthoringTurnPlanner,
    type AuthoringTurnPlannerContext,
} from "./authoringTurn.ts";
import { OpenAiCompatibleClient, type OpenAiCompatibleMessage } from "./openAiCompatibleProvider.ts";

export type OpenAiAuthoringPlannerOptions = {
    client: OpenAiCompatibleClient;
    temperature?: number;
    extraRequestFields?: Record<string, unknown>;
};

export function createOpenAiCompatibleAuthoringPlanner(
    options: OpenAiAuthoringPlannerOptions,
): AuthoringTurnPlanner {
    return async (context) => {
        const completion = await options.client.chat({
            messages: buildPlannerMessages(context),
            temperature: options.temperature,
            extraRequestFields: options.extraRequestFields,
        });
        return applyPlannerGuardrails(parseAuthoringPlanFromModelText(completion.content), context.intent);
    };
}

export function parseAuthoringPlanFromModelText(text: string): AuthoringPlan {
    const jsonText = extractJsonObjectText(text);
    const candidate = JSON.parse(jsonText) as unknown;
    try {
        return normalizeAuthoringPlan(candidate);
    } catch (error) {
        const sanitized = sanitizeModelAuthoredPlan(candidate);
        if (!sanitized.changed) {
            throw error;
        }
        try {
            return normalizeAuthoringPlan(sanitized.plan);
        } catch (sanitizeError) {
            throw new Error(
                `${errorMessage(sanitizeError)} Sanitized model plan: ` +
                truncate(JSON.stringify(sanitized.plan), 2048),
            );
        }
    }
}

function buildPlannerMessages(context: AuthoringTurnPlannerContext): OpenAiCompatibleMessage[] {
    return [
        {
            role: "system",
            content: [
                "You are Luna's game-engine authoring planner.",
                "Return only one JSON object that conforms to the Luna AuthoringPlan protocol.",
                "Do not return markdown, prose, comments, or tool calls.",
                "Use only operations listed in the provided capabilities.",
                "Command objects must contain only protocol command fields; do not copy capability metadata.",
                "Vec3 command fields must be arrays like [x, y, z], not objects like {x, y, z}.",
                "If the user asks to create, start, or build a scene from scratch, make the first command {\"op\":\"new\"}.",
                "If the user asks to add to or modify the current scene, do not use {\"op\":\"new\"} unless explicitly requested.",
                "Prefer small, verifiable plans with inspect/verify/snapshot commands at the end.",
                "If a previous attempt failed, fix the plan using the repair diagnostics.",
            ].join("\n"),
        },
        ...(context.conversation ?? []),
        {
            role: "user",
            content: JSON.stringify({
                intent: context.intent,
                project: context.project ?? null,
                attempt: context.attempt,
                maxAttempts: context.maxAttempts,
                session: context.session,
                capabilities: context.capabilities,
                previousAttempt: context.previousAttempt == null
                    ? null
                    : {
                        ok: context.previousAttempt.ok,
                        repair: context.previousAttempt.repair,
                        report: context.previousAttempt.report,
                        plan: context.previousAttempt.plan,
                    },
                outputExample: writeAuthoringPlanJson({ commands: [{ op: "new" }, { op: "snapshot" }] }).trim(),
            }),
        },
    ];
}

function extractJsonObjectText(text: string): string {
    const trimmed = text.trim();
    const fenceMatch = /^```(?:json)?\s*([\s\S]*?)\s*```$/i.exec(trimmed);
    const candidate = fenceMatch?.[1]?.trim() ?? trimmed;
    if (candidate.startsWith("{") && candidate.endsWith("}")) {
        return candidate;
    }

    const firstBrace = candidate.indexOf("{");
    const lastBrace = candidate.lastIndexOf("}");
    if (firstBrace >= 0 && lastBrace > firstBrace) {
        return candidate.slice(firstBrace, lastBrace + 1);
    }

    throw new Error("AI planner response did not contain an AuthoringPlan JSON object.");
}

const MODEL_COMMAND_METADATA_FIELDS = new Set([
    "title",
    "description",
    "effects",
    "requiresConfirmation",
    "requires_confirmation",
    "parameters",
    "examples",
    "notes",
    "reason",
    "rationale",
]);

const MODEL_COMMAND_VEC3_FIELDS = new Set([
    "translation",
    "rotationDeg",
    "scale",
    "color",
]);

function sanitizeModelAuthoredPlan(input: unknown): { plan: unknown; changed: boolean } {
    if (typeof input !== "object" || input == null || Array.isArray(input)) {
        return { plan: input, changed: false };
    }

    const record = input as Record<string, unknown>;
    if (!Array.isArray(record.commands)) {
        return { plan: input, changed: false };
    }

    let changed = hasUnsupportedPlanRootMetadata(record);
    const commands = record.commands.map((command) => {
        const sanitized = sanitizeModelAuthoredCommand(command);
        changed = changed || sanitized.changed;
        return sanitized.command;
    });
    if (!changed) {
        return { plan: input, changed: false };
    }

    return {
        plan: {
            ...(record.protocol == null ? {} : { protocol: record.protocol }),
            ...(record.project == null ? {} : { project: record.project }),
            commands,
        },
        changed: true,
    };
}

function hasUnsupportedPlanRootMetadata(record: Record<string, unknown>): boolean {
    return Object.keys(record).some((key) => key !== "protocol" && key !== "project" && key !== "commands");
}

function sanitizeModelAuthoredCommand(command: unknown): { command: unknown; changed: boolean } {
    if (typeof command !== "object" || command == null || Array.isArray(command)) {
        return { command, changed: false };
    }

    const record = command as Record<string, unknown>;
    let changed = false;
    const sanitized: Record<string, unknown> = {};
    for (const [key, value] of Object.entries(record)) {
        if (MODEL_COMMAND_METADATA_FIELDS.has(key)) {
            changed = true;
            continue;
        }
        if (MODEL_COMMAND_VEC3_FIELDS.has(key)) {
            const vec3 = modelVec3ObjectToArray(value);
            if (vec3 != null) {
                sanitized[key] = vec3;
                changed = true;
                continue;
            }
        }
        sanitized[key] = value;
    }

    return { command: changed ? sanitized : command, changed };
}

function modelVec3ObjectToArray(value: unknown, depth = 0): [number, number, number] | undefined {
    if (depth > 2) {
        return undefined;
    }

    if (typeof value === "string") {
        const parts = value.trim().split(/[\s,]+/).filter((part) => part.length > 0);
        if (parts.length === 3) {
            const x = modelNumber(parts[0]);
            const y = modelNumber(parts[1]);
            const z = modelNumber(parts[2]);
            if (x != null && y != null && z != null) {
                return [x, y, z];
            }
        }
        return undefined;
    }

    if (Array.isArray(value) && value.length === 3) {
        const x = modelNumber(value[0]);
        const y = modelNumber(value[1]);
        const z = modelNumber(value[2]);
        if (x != null && y != null && z != null) {
            return [x, y, z];
        }
        return undefined;
    }

    if (typeof value !== "object" || value == null || Array.isArray(value)) {
        return undefined;
    }

    const record = value as Record<string, unknown>;
    for (const key of ["value", "values", "array", "vec3", "vector", "position", "translation", "rgb", "color"]) {
        if (record[key] == null) {
            continue;
        }
        const nested = modelVec3ObjectToArray(record[key], depth + 1);
        if (nested != null) {
            return nested;
        }
    }

    const x = modelNumber(record.x ?? record.X ?? record[0] ?? record["0"]);
    const y = modelNumber(record.y ?? record.Y ?? record[1] ?? record["1"]);
    const z = modelNumber(record.z ?? record.Z ?? record[2] ?? record["2"]);
    if (x != null && y != null && z != null) {
        return [x, y, z];
    }

    const r = modelNumber(record.r ?? record.R);
    const g = modelNumber(record.g ?? record.G);
    const b = modelNumber(record.b ?? record.B);
    if (r != null && g != null && b != null) {
        return [r, g, b];
    }
    return undefined;
}

function modelNumber(value: unknown): number | undefined {
    if (typeof value === "number" && Number.isFinite(value)) {
        return value;
    }
    if (typeof value === "string" && value.trim().length > 0) {
        const parsed = Number(value);
        return Number.isFinite(parsed) ? parsed : undefined;
    }
    return undefined;
}

function applyPlannerGuardrails(plan: AuthoringPlan, intent: string): AuthoringPlan {
    if (shouldStartFreshScene(intent) && !startsWithSceneBoundary(plan)) {
        return normalizeAuthoringPlan({
            protocol: plan.protocol,
            project: plan.project,
            commands: [{ op: "new" }, ...plan.commands],
        });
    }
    return plan;
}

function startsWithSceneBoundary(plan: AuthoringPlan): boolean {
    const firstOp = plan.commands[0]?.op;
    return firstOp === "new" || firstOp === "open";
}

function shouldStartFreshScene(intent: string): boolean {
    const normalized = intent.toLowerCase();
    if (containsAny(normalized, [
        "current scene",
        "existing scene",
        "do not create a new scene",
        "add ",
        "modify ",
        "move ",
        "open the saved scene",
        "当前场景",
        "已有场景",
        "不要创建新场景",
        "添加",
        "修改",
        "移动",
        "打开",
    ])) {
        return false;
    }

    return containsAny(normalized, [
        "new scene",
        "start a new scene",
        "create a new scene",
        "create a minimal scene",
        "create a simple scene",
        "build a scene",
        "from scratch",
        "新场景",
        "创建一个",
        "创建场景",
        "最简单的场景",
    ]);
}

function containsAny(text: string, needles: string[]): boolean {
    return needles.some((needle) => text.includes(needle));
}

function errorMessage(error: unknown): string {
    return error instanceof Error ? error.message : String(error);
}

function truncate(text: string, maxLength: number): string {
    return text.length <= maxLength ? text : `${text.slice(0, maxLength)}...`;
}
