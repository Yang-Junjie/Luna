export type OpenAiCompatibleMessage = {
    role: "system" | "user" | "assistant";
    content: string;
};

export type OpenAiCompatibleClientOptions = {
    baseURL: string;
    apiKey: string;
    model: string;
    timeoutMs?: number;
    extraRequestFields?: Record<string, unknown>;
};

export type OpenAiCompatibleChatRequest = {
    messages: OpenAiCompatibleMessage[];
    temperature?: number;
    extraRequestFields?: Record<string, unknown>;
};

export type OpenAiCompatibleChatResult = {
    content: string;
    raw: unknown;
};

export class OpenAiCompatibleClient {
    #baseURL: string;
    #apiKey: string;
    #model: string;
    #timeoutMs: number;
    #extraRequestFields: Record<string, unknown>;

    constructor(options: OpenAiCompatibleClientOptions) {
        this.#baseURL = requireNonEmpty(options.baseURL, "baseURL").replace(/\/+$/, "");
        this.#apiKey = requireNonEmpty(options.apiKey, "apiKey");
        this.#model = requireNonEmpty(options.model, "model");
        this.#timeoutMs = options.timeoutMs ?? 300000;
        this.#extraRequestFields = { ...(options.extraRequestFields ?? {}) };
    }

    async chat(request: OpenAiCompatibleChatRequest): Promise<OpenAiCompatibleChatResult> {
        const controller = new AbortController();
        const timer = this.#timeoutMs > 0
            ? setTimeout(() => controller.abort(), this.#timeoutMs)
            : undefined;
        timer?.unref?.();

        try {
            const response = await fetch(`${this.#baseURL}/chat/completions`, {
                method: "POST",
                headers: {
                    "Authorization": `Bearer ${this.#apiKey}`,
                    "Content-Type": "application/json",
                },
                body: JSON.stringify({
                    model: this.#model,
                    messages: request.messages,
                    stream: false,
                    ...this.#extraRequestFields,
                    ...(request.temperature == null ? {} : { temperature: request.temperature }),
                    ...(request.extraRequestFields ?? {}),
                }),
                signal: controller.signal,
            });

            const text = await response.text();
            if (!response.ok) {
                throw new Error(`OpenAI-compatible provider returned HTTP ${response.status}: ${truncate(text, 4096)}`);
            }

            let payload: unknown;
            try {
                payload = JSON.parse(text) as unknown;
            } catch (error) {
                throw new Error(`OpenAI-compatible provider returned invalid JSON: ${errorMessage(error)}`);
            }

            return {
                content: extractAssistantContent(payload),
                raw: payload,
            };
        } catch (error) {
            if (error instanceof Error && error.name === "AbortError") {
                throw new Error(`OpenAI-compatible provider request timed out after ${this.#timeoutMs}ms. ` +
                    "Increase --ai-timeout-ms or LUNA_AI_TIMEOUT_MS for slower models.");
            }
            throw error;
        } finally {
            if (timer != null) {
                clearTimeout(timer);
            }
        }
    }
}

function requireNonEmpty(value: string, field: string): string {
    if (typeof value !== "string" || value.trim().length === 0) {
        throw new Error(`OpenAI-compatible provider option '${field}' must be a non-empty string.`);
    }
    return value;
}

function requireRecord(value: unknown, field: string): Record<string, unknown> {
    if (typeof value !== "object" || value == null || Array.isArray(value)) {
        throw new Error(`OpenAI-compatible response field '${field}' must be an object.`);
    }
    return value as Record<string, unknown>;
}

function extractAssistantContent(payload: unknown): string {
    const root = requireRecord(payload, "root");
    if (!Array.isArray(root.choices) || root.choices.length === 0) {
        throw new Error("OpenAI-compatible response must include at least one choice.");
    }

    const choice = requireRecord(root.choices[0], "choices[0]");
    const message = requireRecord(choice.message, "choices[0].message");
    const content = message.content;
    if (typeof content === "string") {
        return content;
    }

    if (Array.isArray(content)) {
        const parts = content
            .map((part) => extractContentPart(part))
            .filter((part) => part.length > 0);
        if (parts.length > 0) {
            return parts.join("\n");
        }
    }

    throw new Error("OpenAI-compatible response choice did not contain assistant text content.");
}

function extractContentPart(part: unknown): string {
    if (typeof part === "string") {
        return part;
    }
    if (typeof part !== "object" || part == null || Array.isArray(part)) {
        return "";
    }

    const record = part as Record<string, unknown>;
    if (typeof record.text === "string") {
        return record.text;
    }
    if (typeof record.content === "string") {
        return record.content;
    }
    return "";
}

function truncate(text: string, maxLength: number): string {
    return text.length <= maxLength ? text : `${text.slice(0, maxLength)}...`;
}

function errorMessage(error: unknown): string {
    return error instanceof Error ? error.message : String(error);
}
