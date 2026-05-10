import { createEditorProtocolInfo, type EditorProtocolInfo, type EditorViewportId } from "../protocol/editorProtocol.ts";
import {
    applyEditorViewportCommand,
    createEditorViewportState,
    type EditorViewportCommand,
    type EditorViewportCommandResult,
    type EditorViewportState,
} from "../protocol/viewProtocol.ts";
import type { AuthoringCapabilitiesDocument, AuthoringPlan, AuthoringReport } from "../core/authoringProtocol.ts";
import type { AuthoringHostSessionState } from "../core/authoringHostClient.ts";

export type EditorShellAuthoringState = {
    capabilities: AuthoringCapabilitiesDocument | null;
    session: AuthoringHostSessionState | null;
    lastPlan: AuthoringPlan | null;
    lastReport: AuthoringReport | null;
};

export type EditorShellState = {
    protocol: EditorProtocolInfo;
    authoring: EditorShellAuthoringState;
    activeViewportId: EditorViewportId | null;
    viewports: Record<string, EditorViewportState>;
};

export type EditorShellOptions = {
    protocol?: EditorProtocolInfo;
    activeViewportId?: EditorViewportId | null;
    authoring?: Partial<EditorShellAuthoringState>;
    initialViewports?: EditorViewportState[];
};

export function createEditorShellState(options: EditorShellOptions = {}): EditorShellState {
    const protocol = options.protocol ?? createEditorProtocolInfo();
    const viewports: Record<string, EditorViewportState> = {};
    const initialViewports = options.initialViewports ?? [createEditorViewportState(1, 1)];
    for (const viewport of initialViewports) {
        viewports[String(viewport.viewportId)] = cloneEditorViewportState(viewport);
    }

    const activeViewportId = options.activeViewportId ?? initialViewports[0]?.viewportId ?? null;
    return {
        protocol,
        authoring: {
            capabilities: cloneSerializable(options.authoring?.capabilities ?? null),
            session: cloneSerializable(options.authoring?.session ?? null),
            lastPlan: cloneSerializable(options.authoring?.lastPlan ?? null),
            lastReport: cloneSerializable(options.authoring?.lastReport ?? null),
        },
        activeViewportId,
        viewports,
    };
}

export class EditorShellStore {
    #state: EditorShellState;

    constructor(options: EditorShellOptions = {}) {
        this.#state = createEditorShellState(options);
    }

    state(): EditorShellState {
        return cloneEditorShellState(this.#state);
    }

    setAuthoringCapabilities(capabilities: AuthoringCapabilitiesDocument | null): void {
        this.#state.authoring.capabilities = cloneSerializable(capabilities);
    }

    setAuthoringSession(session: AuthoringHostSessionState | null): void {
        this.#state.authoring.session = cloneSerializable(session);
    }

    setLastPlan(plan: AuthoringPlan | null): void {
        this.#state.authoring.lastPlan = cloneSerializable(plan);
    }

    setLastReport(report: AuthoringReport | null): void {
        this.#state.authoring.lastReport = cloneSerializable(report);
    }

    setActiveViewportId(viewportId: EditorViewportId | null): void {
        this.#state.activeViewportId = viewportId;
    }

    viewport(viewportId: EditorViewportId): EditorViewportState | undefined {
        const viewport = this.#state.viewports[String(viewportId)];
        return viewport == null ? undefined : cloneEditorViewportState(viewport);
    }

    ensureViewport(viewportId: EditorViewportId, planeId = viewportId): EditorViewportState {
        const key = String(viewportId);
        if (this.#state.viewports[key] == null) {
            this.#state.viewports[key] = createEditorViewportState(viewportId, planeId);
        }
        return this.#state.viewports[key];
    }

    removeViewport(viewportId: EditorViewportId): boolean {
        const key = String(viewportId);
        if (this.#state.viewports[key] == null) {
            return false;
        }
        delete this.#state.viewports[key];
        if (this.#state.activeViewportId === viewportId) {
            this.#state.activeViewportId = null;
        }
        return true;
    }

    applyViewportCommand(command: EditorViewportCommand): EditorViewportCommandResult {
        const key = String(command.viewportId);
        const viewport = this.ensureViewport(command.viewportId);
        const previousViewportId = viewport.viewportId;
        const result = applyEditorViewportCommand(viewport, command);
        if (command.kind === "createViewport" && String(viewport.viewportId) !== key) {
            delete this.#state.viewports[key];
            this.#state.viewports[String(viewport.viewportId)] = viewport;
        }

        if (command.kind === "destroyViewport" && this.#state.activeViewportId === previousViewportId) {
            this.#state.activeViewportId = null;
        } else if (command.kind === "createViewport" || this.#state.activeViewportId == null) {
            this.#state.activeViewportId = viewport.viewportId;
        }
        return result;
    }
}

export function getEditorShellViewport(
    state: EditorShellState,
    viewportId: EditorViewportId,
): EditorViewportState | undefined {
    const viewport = state.viewports[String(viewportId)];
    return viewport == null ? undefined : cloneEditorViewportState(viewport);
}

export function listEditorShellViewports(state: EditorShellState): EditorViewportState[] {
    return Object.values(state.viewports)
        .map((viewport) => cloneEditorViewportState(viewport))
        .sort((lhs, rhs) => lhs.viewportId - rhs.viewportId);
}

export function cloneEditorShellState(state: EditorShellState): EditorShellState {
    return {
        protocol: { ...state.protocol },
        authoring: {
            capabilities: cloneSerializable(state.authoring.capabilities),
            session: cloneSerializable(state.authoring.session),
            lastPlan: cloneSerializable(state.authoring.lastPlan),
            lastReport: cloneSerializable(state.authoring.lastReport),
        },
        activeViewportId: state.activeViewportId,
        viewports: Object.fromEntries(
            Object.entries(state.viewports).map(([key, viewport]) => [key, cloneEditorViewportState(viewport)]),
        ),
    };
}

export function cloneEditorViewportState(state: EditorViewportState): EditorViewportState {
    return {
        viewportId: state.viewportId,
        title: state.title,
        kind: state.kind,
        size: { ...state.size },
        camera: {
            ...state.camera,
            position: [...state.camera.position] as [number, number, number],
            orientationEulerRadians: [...state.camera.orientationEulerRadians] as [number, number, number],
        },
        interaction: { ...state.interaction },
        transformTool: state.transformTool,
        transformSpace: state.transformSpace,
        debugViewMode: state.debugViewMode,
        debugVelocityScale: state.debugVelocityScale,
        renderPlane: {
            descriptor: { ...state.renderPlane.descriptor },
            frame: { ...state.renderPlane.frame },
            active: state.renderPlane.active,
        },
    };
}

function cloneSerializable<T>(value: T): T {
    if (value == null) {
        return value;
    }

    const structuredCloneFn = (globalThis as { structuredClone?: <U>(input: U) => U }).structuredClone;
    if (typeof structuredCloneFn === "function") {
        return structuredCloneFn(value);
    }

    return JSON.parse(JSON.stringify(value)) as T;
}
