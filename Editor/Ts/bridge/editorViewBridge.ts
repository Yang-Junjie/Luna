import {
    createEditorProtocolInfo,
    isEditorProtocolCompatible,
    type EditorFrameId,
    type EditorProtocolInfo,
    type EditorViewportId,
} from "../protocol/editorProtocol.ts";
import {
    EDITOR_DEBUG_VIEW_MODES,
    EDITOR_TRANSFORM_SPACES,
    EDITOR_TRANSFORM_TOOLS,
    EDITOR_VIEWPORT_KINDS,
    type EditorDebugViewMode,
    type EditorTransformSpace,
    type EditorTransformTool,
    type EditorViewportCommand,
    type EditorViewportCommandResult,
    type EditorViewportKind,
    type EditorViewportState,
} from "../protocol/viewProtocol.ts";
import {
    EDITOR_RENDER_PLANE_KINDS,
    EDITOR_RENDER_TRANSPORT_KINDS,
    type EditorRenderFrameInfo,
    type EditorRenderPlaneDescriptor,
    type EditorRenderPlaneKind,
    type EditorRenderPlaneState,
    type EditorRenderTransportKind,
} from "../protocol/renderDataPlane.ts";
import type { Vec3 } from "../core/authoringProtocol.ts";

export type EditorViewBridgeDocument = {
    protocol: EditorProtocolInfo;
    activeViewportId: EditorViewportId | null;
    viewports: Record<string, EditorViewportState>;
};

export type EditorRenderFramePresentation = {
    frameId: EditorFrameId;
    sequence: number;
    timestampNs: number;
};

export type EditorViewBridgeCommandResult = EditorViewportCommandResult & {
    document: EditorViewBridgeDocument;
};

export type EditorViewBridgeRenderPlaneResult = {
    ok: boolean;
    changed: boolean;
    document: EditorViewBridgeDocument;
};

export interface EditorViewBridge {
    snapshot(): Promise<EditorViewBridgeDocument>;
    applyViewportCommand(command: EditorViewportCommand): Promise<EditorViewBridgeCommandResult>;
    bindRenderPlane(
        viewportId: EditorViewportId,
        descriptor: EditorRenderPlaneDescriptor,
    ): Promise<EditorViewBridgeRenderPlaneResult>;
    presentRenderPlaneFrame(
        viewportId: EditorViewportId,
        frame: EditorRenderFramePresentation,
    ): Promise<EditorViewBridgeRenderPlaneResult>;
    releaseRenderPlane(viewportId: EditorViewportId): Promise<EditorViewBridgeRenderPlaneResult>;
    close(): Promise<void>;
}

export function createEmptyEditorViewBridgeDocument(): EditorViewBridgeDocument {
    return {
        protocol: createEditorProtocolInfo(),
        activeViewportId: null,
        viewports: {},
    };
}

export function cloneEditorViewBridgeDocument(document: EditorViewBridgeDocument): EditorViewBridgeDocument {
    return {
        protocol: { ...document.protocol },
        activeViewportId: document.activeViewportId,
        viewports: Object.fromEntries(
            Object.entries(document.viewports).map(([key, viewport]) => [key, cloneViewportState(viewport)]),
        ),
    };
}

export function normalizeEditorViewBridgeDocument(value: unknown, field = "document"): EditorViewBridgeDocument {
    const record = requireRecord(value, field);
    const protocol = normalizeEditorProtocolInfo(record.protocol, `${field}.protocol`);
    const activeViewportId = record.activeViewportId == null
        ? null
        : requireNonNegativeInteger(record.activeViewportId, `${field}.activeViewportId`);
    const viewports = requireRecord(record.viewports, `${field}.viewports`);
    const normalizedViewports: Record<string, EditorViewportState> = {};
    for (const [key, viewport] of Object.entries(viewports)) {
        normalizedViewports[key] = normalizeEditorViewportState(viewport, `${field}.viewports.${key}`);
    }

    return {
        protocol,
        activeViewportId,
        viewports: normalizedViewports,
    };
}

export function normalizeEditorViewportCommandResult(
    value: unknown,
    field = "result",
): EditorViewportCommandResult {
    const record = requireRecord(value, field);
    return {
        accepted: requireBoolean(record.accepted, `${field}.accepted`),
        changed: requireBoolean(record.changed, `${field}.changed`),
        requestPick: requireBoolean(record.requestPick, `${field}.requestPick`),
        requestCapture: requireBoolean(record.requestCapture, `${field}.requestCapture`),
    };
}

export function normalizeEditorRenderPlaneResult(
    value: unknown,
    field = "result",
): EditorViewBridgeRenderPlaneResult {
    const record = requireRecord(value, field);
    return {
        ok: requireBoolean(record.ok, `${field}.ok`),
        changed: requireBoolean(record.changed, `${field}.changed`),
        document: normalizeEditorViewBridgeDocument(record.document, `${field}.document`),
    };
}

export function normalizeEditorViewBridgeCommandResult(
    value: unknown,
    field = "result",
): EditorViewBridgeCommandResult {
    const record = requireRecord(value, field);
    const commandResult = normalizeEditorViewportCommandResult(record.result, `${field}.result`);
    return {
        ...commandResult,
        document: normalizeEditorViewBridgeDocument(record.document, `${field}.document`),
    };
}

export function editorShellStateToViewBridgeDocument(state: {
    protocol: EditorProtocolInfo;
    activeViewportId: EditorViewportId | null;
    viewports: Record<string, EditorViewportState>;
}): EditorViewBridgeDocument {
    return cloneEditorViewBridgeDocument({
        protocol: state.protocol,
        activeViewportId: state.activeViewportId,
        viewports: state.viewports,
    });
}

function normalizeEditorProtocolInfo(value: unknown, field: string): EditorProtocolInfo {
    if (!isEditorProtocolCompatible(value)) {
        throw new Error(`Field '${field}' must be the current Luna editor protocol.`);
    }
    return { name: value.name, version: value.version };
}

function normalizeEditorViewportState(value: unknown, field: string): EditorViewportState {
    const record = requireRecord(value, field);
    return {
        viewportId: requireNonNegativeInteger(record.viewportId, `${field}.viewportId`),
        title: requireString(record.title, `${field}.title`),
        kind: requireEnum(record.kind, EDITOR_VIEWPORT_KINDS, `${field}.kind`),
        size: normalizeViewportSize(record.size, `${field}.size`),
        camera: normalizeCameraState(record.camera, `${field}.camera`),
        interaction: normalizeInteractionState(record.interaction, `${field}.interaction`),
        transformTool: requireEnum(record.transformTool, EDITOR_TRANSFORM_TOOLS, `${field}.transformTool`),
        transformSpace: requireEnum(record.transformSpace, EDITOR_TRANSFORM_SPACES, `${field}.transformSpace`),
        debugViewMode: requireEnum(record.debugViewMode, EDITOR_DEBUG_VIEW_MODES, `${field}.debugViewMode`),
        debugVelocityScale: requireFiniteNumber(record.debugVelocityScale, `${field}.debugVelocityScale`),
        renderPlane: normalizeRenderPlaneState(record.renderPlane, `${field}.renderPlane`),
    };
}

function normalizeViewportSize(value: unknown, field: string): { width: number; height: number } {
    const record = requireRecord(value, field);
    return {
        width: requireNonNegativeInteger(record.width, `${field}.width`),
        height: requireNonNegativeInteger(record.height, `${field}.height`),
    };
}

function normalizeCameraState(value: unknown, field: string): EditorViewportState["camera"] {
    const record = requireRecord(value, field);
    return {
        projection: requireEnum(record.projection, ["perspective", "orthographic"] as const, `${field}.projection`),
        position: normalizeVec3(record.position, `${field}.position`),
        orientationEulerRadians: normalizeVec3(record.orientationEulerRadians, `${field}.orientationEulerRadians`),
        perspectiveVerticalFovRadians: requireFiniteNumber(
            record.perspectiveVerticalFovRadians,
            `${field}.perspectiveVerticalFovRadians`,
        ),
        perspectiveNear: requireFiniteNumber(record.perspectiveNear, `${field}.perspectiveNear`),
        perspectiveFar: requireFiniteNumber(record.perspectiveFar, `${field}.perspectiveFar`),
        orthographicSize: requireFiniteNumber(record.orthographicSize, `${field}.orthographicSize`),
        orthographicNear: requireFiniteNumber(record.orthographicNear, `${field}.orthographicNear`),
        orthographicFar: requireFiniteNumber(record.orthographicFar, `${field}.orthographicFar`),
    };
}

function normalizeInteractionState(value: unknown, field: string): EditorViewportState["interaction"] {
    const record = requireRecord(value, field);
    return {
        visible: requireBoolean(record.visible, `${field}.visible`),
        focused: requireBoolean(record.focused, `${field}.focused`),
        hovered: requireBoolean(record.hovered, `${field}.hovered`),
        inputEnabled: requireBoolean(record.inputEnabled, `${field}.inputEnabled`),
        mouseCaptured: requireBoolean(record.mouseCaptured, `${field}.mouseCaptured`),
        runtimeViewport: requireBoolean(record.runtimeViewport, `${field}.runtimeViewport`),
        pickDebugEnabled: requireBoolean(record.pickDebugEnabled, `${field}.pickDebugEnabled`),
    };
}

function normalizeRenderPlaneState(value: unknown, field: string): EditorRenderPlaneState {
    const record = requireRecord(value, field);
    return {
        descriptor: normalizeRenderPlaneDescriptor(record.descriptor, `${field}.descriptor`),
        frame: normalizeRenderFrameInfo(record.frame, `${field}.frame`),
        active: requireBoolean(record.active, `${field}.active`),
    };
}

function normalizeRenderPlaneDescriptor(value: unknown, field: string): EditorRenderPlaneDescriptor {
    const record = requireRecord(value, field);
    return {
        planeId: requireNonNegativeInteger(record.planeId, `${field}.planeId`),
        viewportId: requireNonNegativeInteger(record.viewportId, `${field}.viewportId`),
        kind: requireEnum(record.kind, EDITOR_RENDER_PLANE_KINDS, `${field}.kind`),
        transport: requireEnum(record.transport, EDITOR_RENDER_TRANSPORT_KINDS, `${field}.transport`),
        format: requireString(record.format, `${field}.format`),
        width: requireNonNegativeInteger(record.width, `${field}.width`),
        height: requireNonNegativeInteger(record.height, `${field}.height`),
        yFlip: requireBoolean(record.yFlip, `${field}.yFlip`),
        presentable: requireBoolean(record.presentable, `${field}.presentable`),
        generation: requireNonNegativeInteger(record.generation, `${field}.generation`),
        bindingToken: requireString(record.bindingToken, `${field}.bindingToken`),
        label: requireString(record.label, `${field}.label`),
    };
}

function normalizeRenderFrameInfo(value: unknown, field: string): EditorRenderFrameInfo {
    const record = requireRecord(value, field);
    return {
        frameId: requireNonNegativeInteger(record.frameId, `${field}.frameId`),
        planeId: requireNonNegativeInteger(record.planeId, `${field}.planeId`),
        sequence: requireNonNegativeInteger(record.sequence, `${field}.sequence`),
        timestampNs: requireNonNegativeInteger(record.timestampNs, `${field}.timestampNs`),
        width: requireNonNegativeInteger(record.width, `${field}.width`),
        height: requireNonNegativeInteger(record.height, `${field}.height`),
        ready: requireBoolean(record.ready, `${field}.ready`),
    };
}

function normalizeVec3(value: unknown, field: string): Vec3 {
    if (!Array.isArray(value) || value.length !== 3) {
        throw new Error(`Field '${field}' must be a 3-number array.`);
    }
    return [
        requireFiniteNumber(value[0], `${field}[0]`),
        requireFiniteNumber(value[1], `${field}[1]`),
        requireFiniteNumber(value[2], `${field}[2]`),
    ];
}

function requireRecord(value: unknown, field: string): Record<string, unknown> {
    if (value == null || typeof value !== "object" || Array.isArray(value)) {
        throw new Error(`Field '${field}' must be an object.`);
    }
    return value as Record<string, unknown>;
}

function requireString(value: unknown, field: string): string {
    if (typeof value !== "string") {
        throw new Error(`Field '${field}' must be a string.`);
    }
    return value;
}

function requireBoolean(value: unknown, field: string): boolean {
    if (typeof value !== "boolean") {
        throw new Error(`Field '${field}' must be a boolean.`);
    }
    return value;
}

function requireFiniteNumber(value: unknown, field: string): number {
    if (typeof value !== "number" || !Number.isFinite(value)) {
        throw new Error(`Field '${field}' must be a finite number.`);
    }
    return value;
}

function requireNonNegativeInteger(value: unknown, field: string): number {
    const number = requireFiniteNumber(value, field);
    if (!Number.isInteger(number) || number < 0) {
        throw new Error(`Field '${field}' must be a non-negative integer.`);
    }
    return number;
}

function requireEnum<const T extends readonly string[]>(value: unknown, allowed: T, field: string): T[number] {
    if (typeof value !== "string" || !allowed.includes(value)) {
        throw new Error(`Field '${field}' has unsupported value '${String(value)}'.`);
    }
    return value as T[number];
}

function cloneViewportState(state: EditorViewportState): EditorViewportState {
    return {
        viewportId: state.viewportId,
        title: state.title,
        kind: state.kind as EditorViewportKind,
        size: { ...state.size },
        camera: {
            ...state.camera,
            position: [...state.camera.position] as Vec3,
            orientationEulerRadians: [...state.camera.orientationEulerRadians] as Vec3,
        },
        interaction: { ...state.interaction },
        transformTool: state.transformTool as EditorTransformTool,
        transformSpace: state.transformSpace as EditorTransformSpace,
        debugViewMode: state.debugViewMode as EditorDebugViewMode,
        debugVelocityScale: state.debugVelocityScale,
        renderPlane: {
            descriptor: { ...state.renderPlane.descriptor },
            frame: { ...state.renderPlane.frame },
            active: state.renderPlane.active,
        },
    };
}
