import { existsSync, readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

export function loadLunaEnvironment(): string[] {
    const preexistingKeys = new Set(Object.keys(process.env));
    const loadedFiles: string[] = [];

    for (const filePath of findDotEnvFiles()) {
        if (!existsSync(filePath)) {
            continue;
        }

        const loaded = loadDotEnvFile(filePath, preexistingKeys);
        if (loaded) {
            loadedFiles.push(filePath);
        }
    }

    return loadedFiles;
}

function findDotEnvFiles(): string[] {
    const scriptDir = dirname(fileURLToPath(import.meta.url));
    const repoRoot = resolve(scriptDir, "..", "..");
    const candidates = [
        resolve(repoRoot, ".env"),
        resolve(repoRoot, "CLI", ".env"),
        resolve(repoRoot, "CLI", "client", ".env"),
        resolve(process.cwd(), ".env"),
    ];

    return [...new Set(candidates)];
}

function loadDotEnvFile(filePath: string, preexistingKeys: Set<string>): boolean {
    const text = readFileSync(filePath, "utf8").replace(/^\uFEFF/, "");
    const values = parseDotEnv(text, filePath);
    let loadedAny = false;

    for (const [key, value] of Object.entries(values)) {
        if (preexistingKeys.has(key)) {
            continue;
        }
        process.env[key] = value;
        loadedAny = true;
    }

    return loadedAny;
}

function parseDotEnv(text: string, filePath: string): Record<string, string> {
    const values: Record<string, string> = {};
    const lines = text.split(/\r?\n/);

    for (const [index, rawLine] of lines.entries()) {
        const lineNumber = index + 1;
        const trimmed = rawLine.trim();
        if (trimmed.length === 0 || trimmed.startsWith("#")) {
            continue;
        }

        const entry = parseDotEnvLine(trimmed, filePath, lineNumber);
        if (entry != null) {
            values[entry.key] = entry.value;
        }
    }

    return values;
}

function parseDotEnvLine(
    line: string,
    filePath: string,
    lineNumber: number,
): { key: string; value: string } | null {
    const normalized = line.startsWith("export ") ? line.slice("export ".length).trimStart() : line;
    const separator = normalized.indexOf("=");
    if (separator < 0) {
        throw new Error(`Malformed .env line ${lineNumber} in ${filePath}: missing '='.`);
    }

    const key = normalized.slice(0, separator).trim();
    if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(key)) {
        throw new Error(`Malformed .env line ${lineNumber} in ${filePath}: invalid key '${key}'.`);
    }

    const rawValue = normalized.slice(separator + 1).trimStart();
    return {
        key,
        value: parseDotEnvValue(rawValue),
    };
}

function parseDotEnvValue(rawValue: string): string {
    if (rawValue.length === 0) {
        return "";
    }

    const quote = rawValue[0];
    if (quote === "'" || quote === "\"") {
        const closingQuote = findClosingQuote(rawValue, quote);
        if (closingQuote < 0) {
            throw new Error("Malformed .env quoted value: missing closing quote.");
        }

        const quoted = rawValue.slice(1, closingQuote);
        return quote === "\""
            ? decodeDoubleQuotedValue(quoted)
            : quoted.replace(/\\\\/g, "\\").replace(/\\'/g, "'");
    }

    return stripInlineComment(rawValue).trimEnd();
}

function findClosingQuote(value: string, quote: "'" | "\""): number {
    for (let index = 1; index < value.length; ++index) {
        if (value[index] !== quote) {
            continue;
        }

        let slashCount = 0;
        for (let back = index - 1; back >= 0 && value[back] === "\\"; --back) {
            ++slashCount;
        }
        if (slashCount % 2 === 0) {
            return index;
        }
    }

    return -1;
}

function decodeDoubleQuotedValue(value: string): string {
    return value.replace(/\\([\\\"nrt])/g, (_match, escaped: string) => {
        switch (escaped) {
            case "\\":
                return "\\";
            case "\"":
                return "\"";
            case "n":
                return "\n";
            case "r":
                return "\r";
            case "t":
                return "\t";
            default:
                return escaped;
        }
    });
}

function stripInlineComment(value: string): string {
    let inWhitespace = false;
    for (let index = 0; index < value.length; ++index) {
        const char = value[index];
        if (char === "#") {
            if (index === 0 || inWhitespace) {
                return value.slice(0, index).trimEnd();
            }
        }
        inWhitespace = /\s/.test(char);
    }
    return value;
}
