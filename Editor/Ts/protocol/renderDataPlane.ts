import {
    EDITOR_PROTOCOL_NAME,
    EDITOR_PROTOCOL_VERSION,
    type EditorFrameId,
    type EditorRenderPlaneId,
    type EditorViewportId,
} from "./editorProtocol.ts";

export type EditorRenderPlaneKind = "sceneViewport" | "debugViewport" | "preview" | "capture";
export type EditorRenderTransportKind = "none" | "nativeSurface" | "sharedTexture" | "cpuImage";

export type EditorRenderPlaneDescriptor = {
    planeId: EditorRenderPlaneId;
    viewportId: EditorViewportId;
    kind: EditorRenderPlaneKind;
    transport: EditorRenderTransportKind;
    format: string;
    width: number;
    height: number;
    yFlip: boolean;
    presentable: boolean;
    generation: number;
    bindingToken: string;
    label: string;
};

export type EditorRenderFrameInfo = {
    frameId: EditorFrameId;
    planeId: EditorRenderPlaneId;
    sequence: number;
    timestampNs: number;
    width: number;
    height: number;
    ready: boolean;
};

export type EditorRenderPlaneState = {
    descriptor: EditorRenderPlaneDescriptor;
    frame: EditorRenderFrameInfo;
    active: boolean;
};

export const EDITOR_RENDER_PLANE_KINDS = ["sceneViewport", "debugViewport", "preview", "capture"] as const;
export const EDITOR_RENDER_TRANSPORT_KINDS = ["none", "nativeSurface", "sharedTexture", "cpuImage"] as const;

export function createEditorRenderPlaneDescriptor(
    overrides: Partial<EditorRenderPlaneDescriptor> = {},
): EditorRenderPlaneDescriptor {
    return {
        planeId: 0,
        viewportId: 0,
        kind: "sceneViewport",
        transport: "none",
        format: "UNDEFINED",
        width: 0,
        height: 0,
        yFlip: false,
        presentable: false,
        generation: 0,
        bindingToken: "",
        label: "",
        ...overrides,
    };
}

export function createEditorRenderFrameInfo(
    overrides: Partial<EditorRenderFrameInfo> = {},
): EditorRenderFrameInfo {
    return {
        frameId: 0,
        planeId: 0,
        sequence: 0,
        timestampNs: 0,
        width: 0,
        height: 0,
        ready: false,
        ...overrides,
    };
}

export function createEditorRenderPlaneState(
    overrides: Partial<EditorRenderPlaneState> = {},
): EditorRenderPlaneState {
    return {
        descriptor: createEditorRenderPlaneDescriptor(),
        frame: createEditorRenderFrameInfo(),
        active: false,
        ...overrides,
    };
}

export function editorRenderPlaneKindName(kind: EditorRenderPlaneKind): EditorRenderPlaneKind {
    return kind;
}

export function editorRenderTransportKindName(kind: EditorRenderTransportKind): EditorRenderTransportKind {
    return kind;
}

export function editorRenderTransportIsRealtime(kind: EditorRenderTransportKind): boolean {
    return kind === "nativeSurface" || kind === "sharedTexture";
}

export function isEditorRenderPlaneDescriptorValid(descriptor: EditorRenderPlaneDescriptor): boolean {
    return (
        descriptor.planeId !== 0 &&
        descriptor.viewportId !== 0 &&
        descriptor.transport !== "none" &&
        descriptor.width > 0 &&
        descriptor.height > 0 &&
        descriptor.format !== "UNDEFINED"
    );
}

export function isEditorRenderFrameInfoValid(frame: EditorRenderFrameInfo): boolean {
    return frame.frameId !== 0 && frame.planeId !== 0 && frame.width > 0 && frame.height > 0;
}

export function canPresentEditorRenderPlane(state: EditorRenderPlaneState): boolean {
    return state.active && isEditorRenderPlaneDescriptorValid(state.descriptor) && state.descriptor.presentable;
}

export function sameEditorRenderPlaneBinding(
    lhs: EditorRenderPlaneDescriptor,
    rhs: EditorRenderPlaneDescriptor,
): boolean {
    return (
        lhs.planeId === rhs.planeId &&
        lhs.viewportId === rhs.viewportId &&
        lhs.kind === rhs.kind &&
        lhs.transport === rhs.transport &&
        lhs.format === rhs.format &&
        lhs.width === rhs.width &&
        lhs.height === rhs.height &&
        lhs.yFlip === rhs.yFlip &&
        lhs.presentable === rhs.presentable &&
        lhs.bindingToken === rhs.bindingToken &&
        lhs.label === rhs.label
    );
}

export function bindEditorRenderPlane(
    state: EditorRenderPlaneState,
    descriptor: EditorRenderPlaneDescriptor,
): boolean {
    const changed = !sameEditorRenderPlaneBinding(state.descriptor, descriptor);
    const generation = changed ? state.descriptor.generation + 1 : state.descriptor.generation;

    state.descriptor = {
        ...descriptor,
        generation,
    };
    state.active = state.descriptor.transport !== "none" && state.descriptor.width > 0 && state.descriptor.height > 0;
    state.frame.planeId = state.descriptor.planeId;
    if (changed) {
        state.frame.ready = false;
    }
    return changed;
}

export function presentEditorRenderPlaneFrame(
    state: EditorRenderPlaneState,
    frameId: EditorFrameId,
    sequence: number,
    timestampNs: number,
): void {
    state.frame.frameId = frameId;
    state.frame.planeId = state.descriptor.planeId;
    state.frame.sequence = sequence;
    state.frame.timestampNs = timestampNs;
    state.frame.width = state.descriptor.width;
    state.frame.height = state.descriptor.height;
    state.frame.ready = canPresentEditorRenderPlane(state);
}

export function releaseEditorRenderPlane(state: EditorRenderPlaneState): boolean {
    if (
        state.descriptor.transport === "none" &&
        state.descriptor.width === 0 &&
        state.descriptor.height === 0 &&
        state.descriptor.bindingToken === "" &&
        !state.active &&
        !isEditorRenderFrameInfoValid(state.frame)
    ) {
        return false;
    }

    const releasedDescriptor = createEditorRenderPlaneDescriptor({
        planeId: state.descriptor.planeId,
        viewportId: state.descriptor.viewportId,
        kind: state.descriptor.kind,
        generation: state.descriptor.generation + 1,
        label: state.descriptor.label,
    });

    state.descriptor = releasedDescriptor;
    state.frame = createEditorRenderFrameInfo({ planeId: releasedDescriptor.planeId });
    state.active = false;
    return true;
}

export function editorRenderPlaneProtocolInfo(): { name: typeof EDITOR_PROTOCOL_NAME; version: typeof EDITOR_PROTOCOL_VERSION } {
    return {
        name: EDITOR_PROTOCOL_NAME,
        version: EDITOR_PROTOCOL_VERSION,
    };
}
