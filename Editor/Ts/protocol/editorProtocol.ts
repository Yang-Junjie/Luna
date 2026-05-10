export const EDITOR_PROTOCOL_NAME = "luna.editor" as const;
export const EDITOR_PROTOCOL_VERSION = 1 as const;

export type EditorSessionId = number;
export type EditorViewportId = number;
export type EditorRenderPlaneId = number;
export type EditorFrameId = number;

export type EditorProtocolInfo = {
    name: typeof EDITOR_PROTOCOL_NAME;
    version: typeof EDITOR_PROTOCOL_VERSION;
};

export function createEditorProtocolInfo(): EditorProtocolInfo {
    return {
        name: EDITOR_PROTOCOL_NAME,
        version: EDITOR_PROTOCOL_VERSION,
    };
}

export function isEditorProtocolCompatible(value: unknown): value is EditorProtocolInfo {
    if (value == null || typeof value !== "object") {
        return false;
    }

    const record = value as { name?: unknown; version?: unknown };
    return record.name === EDITOR_PROTOCOL_NAME && record.version === EDITOR_PROTOCOL_VERSION;
}
