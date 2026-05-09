import {
    normalizeAuthoringPlan,
    writeAuthoringPlanJson,
    type AuthoringPlan,
} from "./authoringProtocol.ts";
import { type AuthoringTurnPlanner, type AuthoringTurnPlannerContext } from "./authoringTurn.ts";
import { OpenAiCompatibleClient } from "./openAiCompatibleProvider.ts";

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
        return parseAuthoringPlanFromModelText(completion.content);
    };
}

export function parseAuthoringPlanFromModelText(text: string): AuthoringPlan {
    const jsonText = extractJsonObjectText(text);
    return normalizeAuthoringPlan(JSON.parse(jsonText) as unknown);
}

function buildPlannerMessages(context: AuthoringTurnPlannerContext): Array<{ role: "system" | "user"; content: string }> {
    return [
        {
            role: "system",
            content: [
                "You are Luna's game-engine authoring planner.",
                "Return only one JSON object that conforms to the Luna AuthoringPlan protocol.",
                "Do not return markdown, prose, comments, or tool calls.",
                "Use only operations listed in the provided capabilities.",
                "Prefer small, verifiable plans with inspect/verify/snapshot commands at the end.",
                "If a previous attempt failed, fix the plan using the repair diagnostics.",
            ].join("\n"),
        },
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
