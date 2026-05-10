import { type Vec3 } from "../core/authoringProtocol.ts";
import { createEditorProtocolInfo, type EditorViewportId } from "./editorProtocol.ts";
import {
    bindEditorRenderPlane,
    canPresentEditorRenderPlane,
    createEditorRenderPlaneState,
    editorRenderPlaneKindName,
    editorRenderTransportKindName,
    editorRenderTransportIsRealtime,
    presentEditorRenderPlaneFrame,
    releaseEditorRenderPlane,
    sameEditorRenderPlaneBinding,
    type EditorRenderFrameInfo,
    type EditorRenderPlaneDescriptor,
    type EditorRenderPlaneKind,
    type EditorRenderPlaneState,
    type EditorRenderTransportKind,
} from "./renderDataPlane.ts";

export type EditorViewportKind = "scene" | "debug" | "preview" | "custom";
export type EditorTransformTool = "translate" | "rotate" | "scale";
export type EditorTransformSpace = "local" | "world";
export type EditorDebugViewMode =
    | "none"
    | "velocity"
    | "historyValidity"
    | "shadowCascades"
    | "baseColor"
    | "normal"
    | "metallic"
    | "roughness"
    | "directLighting"
    | "specularIbl"
    | "bloomInput"
    | "bloomPrefilter"
    | "bloomMip0"
    | "bloomMip1"
    | "bloomMip2"
    | "bloomMip3"
    | "bloomMip4"
    | "bloomMip5"
    | "bloomComposite";

export type EditorViewportSize = {
    width: number;
    height: number;
};

export type EditorViewportCameraState = {
    projection: "perspective" | "orthographic";
    position: Vec3;
    orientationEulerRadians: Vec3;
    perspectiveVerticalFovRadians: number;
    perspectiveNear: number;
    perspectiveFar: number;
    orthographicSize: number;
    orthographicNear: number;
    orthographicFar: number;
};

export type EditorViewportInteractionState = {
    visible: boolean;
    focused: boolean;
    hovered: boolean;
    inputEnabled: boolean;
    mouseCaptured: boolean;
    runtimeViewport: boolean;
    pickDebugEnabled: boolean;
};

export type EditorViewportState = {
    viewportId: EditorViewportId;
    title: string;
    kind: EditorViewportKind;
    size: EditorViewportSize;
    camera: EditorViewportCameraState;
    interaction: EditorViewportInteractionState;
    transformTool: EditorTransformTool;
    transformSpace: EditorTransformSpace;
    debugViewMode: EditorDebugViewMode;
    debugVelocityScale: number;
    renderPlane: EditorRenderPlaneState;
};

export type EditorViewportCommand =
    | {
          kind: "createViewport";
          viewportId: EditorViewportId;
          title: string;
          viewportKind: EditorViewportKind;
          size: EditorViewportSize;
          camera: EditorViewportCameraState;
          interaction: EditorViewportInteractionState;
          transformTool: EditorTransformTool;
          transformSpace: EditorTransformSpace;
          debugViewMode: EditorDebugViewMode;
          debugVelocityScale: number;
      }
    | { kind: "destroyViewport"; viewportId: EditorViewportId }
    | { kind: "resizeViewport"; viewportId: EditorViewportId; size: EditorViewportSize }
    | { kind: "setCamera"; viewportId: EditorViewportId; camera: EditorViewportCameraState }
    | { kind: "setInteractionState"; viewportId: EditorViewportId; interaction: EditorViewportInteractionState }
    | { kind: "setTransformState"; viewportId: EditorViewportId; transformTool: EditorTransformTool; transformSpace: EditorTransformSpace }
    | { kind: "setDebugViewMode"; viewportId: EditorViewportId; debugViewMode: EditorDebugViewMode }
    | { kind: "setDebugVelocityScale"; viewportId: EditorViewportId; debugVelocityScale: number }
    | { kind: "requestPick"; viewportId: EditorViewportId; pickX: number; pickY: number }
    | { kind: "captureFrame"; viewportId: EditorViewportId };

export type EditorViewportCommandResult = {
    accepted: boolean;
    changed: boolean;
    requestPick: boolean;
    requestCapture: boolean;
};

export const EDITOR_VIEWPORT_KINDS = ["scene", "debug", "preview", "custom"] as const;
export const EDITOR_TRANSFORM_TOOLS = ["translate", "rotate", "scale"] as const;
export const EDITOR_TRANSFORM_SPACES = ["local", "world"] as const;
export const EDITOR_DEBUG_VIEW_MODES = [
    "none",
    "velocity",
    "historyValidity",
    "shadowCascades",
    "baseColor",
    "normal",
    "metallic",
    "roughness",
    "directLighting",
    "specularIbl",
    "bloomInput",
    "bloomPrefilter",
    "bloomMip0",
    "bloomMip1",
    "bloomMip2",
    "bloomMip3",
    "bloomMip4",
    "bloomMip5",
    "bloomComposite",
] as const;

export function createEditorViewportCameraState(
    overrides: Partial<EditorViewportCameraState> = {},
): EditorViewportCameraState {
    return {
        projection: "perspective",
        position: [0, 0, 0],
        orientationEulerRadians: [0, 0, 0],
        perspectiveVerticalFovRadians: (50 * Math.PI) / 180,
        perspectiveNear: 0.05,
        perspectiveFar: 500,
        orthographicSize: 10,
        orthographicNear: -100,
        orthographicFar: 100,
        ...overrides,
    };
}

export function createEditorViewportInteractionState(
    overrides: Partial<EditorViewportInteractionState> = {},
): EditorViewportInteractionState {
    return {
        visible: true,
        focused: false,
        hovered: false,
        inputEnabled: false,
        mouseCaptured: false,
        runtimeViewport: false,
        pickDebugEnabled: false,
        ...overrides,
    };
}

export function createEditorViewportState(
    viewportId = 1,
    planeId = 1,
    overrides: Partial<EditorViewportState> = {},
): EditorViewportState {
    return {
        viewportId,
        title: "Viewport",
        kind: "scene",
        size: { width: 0, height: 0 },
        camera: createEditorViewportCameraState(),
        interaction: createEditorViewportInteractionState(),
        transformTool: "translate",
        transformSpace: "local",
        debugViewMode: "none",
        debugVelocityScale: 20,
        renderPlane: createEditorRenderPlaneState({
            descriptor: {
                ...createEditorRenderPlaneState().descriptor,
                planeId,
                viewportId,
                label: "Viewport",
            },
        }),
        ...overrides,
    };
}

export function sameEditorViewportCameraState(lhs: EditorViewportCameraState, rhs: EditorViewportCameraState): boolean {
    return (
        lhs.projection === rhs.projection &&
        lhs.position[0] === rhs.position[0] &&
        lhs.position[1] === rhs.position[1] &&
        lhs.position[2] === rhs.position[2] &&
        lhs.orientationEulerRadians[0] === rhs.orientationEulerRadians[0] &&
        lhs.orientationEulerRadians[1] === rhs.orientationEulerRadians[1] &&
        lhs.orientationEulerRadians[2] === rhs.orientationEulerRadians[2] &&
        lhs.perspectiveVerticalFovRadians === rhs.perspectiveVerticalFovRadians &&
        lhs.perspectiveNear === rhs.perspectiveNear &&
        lhs.perspectiveFar === rhs.perspectiveFar &&
        lhs.orthographicSize === rhs.orthographicSize &&
        lhs.orthographicNear === rhs.orthographicNear &&
        lhs.orthographicFar === rhs.orthographicFar
    );
}

export function sameEditorViewportInteractionState(
    lhs: EditorViewportInteractionState,
    rhs: EditorViewportInteractionState,
): boolean {
    return (
        lhs.visible === rhs.visible &&
        lhs.focused === rhs.focused &&
        lhs.hovered === rhs.hovered &&
        lhs.inputEnabled === rhs.inputEnabled &&
        lhs.mouseCaptured === rhs.mouseCaptured &&
        lhs.runtimeViewport === rhs.runtimeViewport &&
        lhs.pickDebugEnabled === rhs.pickDebugEnabled
    );
}

export function sameEditorViewportSize(lhs: EditorViewportSize, rhs: EditorViewportSize): boolean {
    return lhs.width === rhs.width && lhs.height === rhs.height;
}

export function sameEditorViewportState(lhs: EditorViewportState, rhs: EditorViewportState): boolean {
    return (
        lhs.viewportId === rhs.viewportId &&
        lhs.title === rhs.title &&
        lhs.kind === rhs.kind &&
        sameEditorViewportSize(lhs.size, rhs.size) &&
        sameEditorViewportCameraState(lhs.camera, rhs.camera) &&
        sameEditorViewportInteractionState(lhs.interaction, rhs.interaction) &&
        lhs.transformTool === rhs.transformTool &&
        lhs.transformSpace === rhs.transformSpace &&
        lhs.debugViewMode === rhs.debugViewMode &&
        lhs.debugVelocityScale === rhs.debugVelocityScale &&
        lhs.renderPlane.descriptor.planeId === rhs.renderPlane.descriptor.planeId &&
        lhs.renderPlane.descriptor.viewportId === rhs.renderPlane.descriptor.viewportId &&
        lhs.renderPlane.descriptor.kind === rhs.renderPlane.descriptor.kind &&
        lhs.renderPlane.descriptor.transport === rhs.renderPlane.descriptor.transport &&
        lhs.renderPlane.descriptor.format === rhs.renderPlane.descriptor.format &&
        lhs.renderPlane.descriptor.width === rhs.renderPlane.descriptor.width &&
        lhs.renderPlane.descriptor.height === rhs.renderPlane.descriptor.height &&
        lhs.renderPlane.descriptor.yFlip === rhs.renderPlane.descriptor.yFlip &&
        lhs.renderPlane.descriptor.presentable === rhs.renderPlane.descriptor.presentable &&
        lhs.renderPlane.descriptor.generation === rhs.renderPlane.descriptor.generation &&
        lhs.renderPlane.descriptor.bindingToken === rhs.renderPlane.descriptor.bindingToken &&
        lhs.renderPlane.descriptor.label === rhs.renderPlane.descriptor.label &&
        lhs.renderPlane.frame.frameId === rhs.renderPlane.frame.frameId &&
        lhs.renderPlane.frame.planeId === rhs.renderPlane.frame.planeId &&
        lhs.renderPlane.frame.sequence === rhs.renderPlane.frame.sequence &&
        lhs.renderPlane.frame.timestampNs === rhs.renderPlane.frame.timestampNs &&
        lhs.renderPlane.frame.width === rhs.renderPlane.frame.width &&
        lhs.renderPlane.frame.height === rhs.renderPlane.frame.height &&
        lhs.renderPlane.frame.ready === rhs.renderPlane.frame.ready &&
        lhs.renderPlane.active === rhs.renderPlane.active
    );
}

export function createEditorViewportCommandResult(
    overrides: Partial<EditorViewportCommandResult> = {},
): EditorViewportCommandResult {
    return {
        accepted: true,
        changed: false,
        requestPick: false,
        requestCapture: false,
        ...overrides,
    };
}

export function editorViewportCommandName(command: EditorViewportCommand["kind"]): EditorViewportCommand["kind"] {
    return command;
}

export function editorViewportKindName(kind: EditorViewportKind): EditorViewportKind {
    return kind;
}

export function editorTransformToolName(tool: EditorTransformTool): EditorTransformTool {
    return tool;
}

export function editorTransformSpaceName(space: EditorTransformSpace): EditorTransformSpace {
    return space;
}

export function editorRenderDebugViewModeName(mode: EditorDebugViewMode): EditorDebugViewMode {
    return mode;
}

export function editorViewportCommandEffects(command: EditorViewportCommand["kind"]): {
    mutatesViewportState: boolean;
    requestsPick: boolean;
    requestsCapture: boolean;
} {
    switch (command) {
        case "requestPick":
            return { mutatesViewportState: false, requestsPick: true, requestsCapture: false };
        case "captureFrame":
            return { mutatesViewportState: false, requestsPick: false, requestsCapture: true };
        default:
            return { mutatesViewportState: true, requestsPick: false, requestsCapture: false };
    }
}

export function applyEditorViewportCommand(
    state: EditorViewportState,
    command: EditorViewportCommand,
): EditorViewportCommandResult {
    const result = createEditorViewportCommandResult({ accepted: true });

    switch (command.kind) {
        case "createViewport": {
            const viewportId = command.viewportId !== 0 ? command.viewportId : state.viewportId !== 0 ? state.viewportId : 1;
            const title = command.title.length === 0 ? "Viewport" : command.title;
            const changed =
                state.viewportId !== viewportId ||
                state.title !== title ||
                state.kind !== command.viewportKind ||
                !sameEditorViewportSize(state.size, command.size) ||
                !sameEditorViewportCameraState(state.camera, command.camera) ||
                !sameEditorViewportInteractionState(state.interaction, command.interaction) ||
                state.transformTool !== command.transformTool ||
                state.transformSpace !== command.transformSpace ||
                state.debugViewMode !== command.debugViewMode ||
                state.debugVelocityScale !== command.debugVelocityScale;

            state.viewportId = viewportId;
            state.title = title;
            state.kind = command.viewportKind;
            state.size = { ...command.size };
            state.camera = {
                ...command.camera,
                position: [...command.camera.position] as Vec3,
                orientationEulerRadians: [...command.camera.orientationEulerRadians] as Vec3,
            };
            state.interaction = { ...command.interaction };
            state.transformTool = command.transformTool;
            state.transformSpace = command.transformSpace;
            state.debugViewMode = command.debugViewMode;
            state.debugVelocityScale = command.debugVelocityScale;
            if (state.renderPlane.descriptor.label !== "" && state.renderPlane.descriptor.label !== state.title) {
                state.renderPlane.descriptor.label = state.title;
            }
            result.changed = changed;
            break;
        }
        case "destroyViewport":
            result.changed =
                state.viewportId !== 0 ||
                state.size.width !== 0 ||
                state.size.height !== 0 ||
                state.title !== "" ||
                state.kind !== "scene" ||
                !sameEditorViewportCameraState(state.camera, createEditorViewportCameraState()) ||
                !sameEditorViewportInteractionState(state.interaction, createEditorViewportInteractionState()) ||
                state.transformTool !== "translate" ||
                state.transformSpace !== "local" ||
                state.debugViewMode !== "none" ||
                state.debugVelocityScale !== 20 ||
                state.renderPlane.active;
            Object.assign(state, createEditorViewportState(0, 0));
            state.renderPlane = createEditorRenderPlaneState();
            break;
        case "resizeViewport":
            result.changed = !sameEditorViewportSize(state.size, command.size);
            state.size = { ...command.size };
            break;
        case "setCamera":
            result.changed = !sameEditorViewportCameraState(state.camera, command.camera);
            state.camera = {
                ...command.camera,
                position: [...command.camera.position] as Vec3,
                orientationEulerRadians: [...command.camera.orientationEulerRadians] as Vec3,
            };
            break;
        case "setInteractionState":
            result.changed = !sameEditorViewportInteractionState(state.interaction, command.interaction);
            state.interaction = { ...command.interaction };
            break;
        case "setTransformState":
            result.changed = state.transformTool !== command.transformTool || state.transformSpace !== command.transformSpace;
            state.transformTool = command.transformTool;
            state.transformSpace = command.transformSpace;
            break;
        case "setDebugViewMode":
            result.changed = state.debugViewMode !== command.debugViewMode;
            state.debugViewMode = command.debugViewMode;
            break;
        case "setDebugVelocityScale":
            result.changed = state.debugVelocityScale !== command.debugVelocityScale;
            state.debugVelocityScale = command.debugVelocityScale;
            break;
        case "requestPick":
            result.requestPick = true;
            result.changed = false;
            break;
        case "captureFrame":
            result.requestCapture = true;
            result.changed = false;
            break;
    }

    return result;
}

export function createEditorViewportProtocolInfo() {
    return createEditorProtocolInfo();
}

export {
    bindEditorRenderPlane,
    canPresentEditorRenderPlane,
    createEditorRenderPlaneState,
    editorRenderPlaneKindName,
    editorRenderTransportKindName,
    editorRenderTransportIsRealtime,
    presentEditorRenderPlaneFrame,
    releaseEditorRenderPlane,
    sameEditorRenderPlaneBinding,
    type EditorRenderFrameInfo,
    type EditorRenderPlaneDescriptor,
    type EditorRenderPlaneKind,
    type EditorRenderPlaneState,
    type EditorRenderTransportKind,
};
