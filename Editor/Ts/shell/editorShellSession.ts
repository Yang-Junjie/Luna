import type { EditorViewportId } from "../protocol/editorProtocol.ts";
import type { EditorRenderPlaneDescriptor } from "../protocol/renderDataPlane.ts";
import type { EditorViewportCommand } from "../protocol/viewProtocol.ts";
import {
    cloneEditorViewBridgeDocument,
    type EditorRenderFramePresentation,
    type EditorViewBridge,
    type EditorViewBridgeCommandResult,
    type EditorViewBridgeDocument,
    type EditorViewBridgeRenderPlaneResult,
} from "../bridge/editorViewBridge.ts";
import type { AuthoringCapabilitiesDocument, AuthoringPlan, AuthoringReport } from "../core/authoringProtocol.ts";
import type { AuthoringHostSessionState } from "../core/authoringHostClient.ts";
import {
    cloneEditorShellState,
    createEditorShellState,
    type EditorShellOptions,
    type EditorShellState,
} from "./editorShell.ts";

export class EditorShellSession {
    #bridge: EditorViewBridge;
    #state: EditorShellState;

    constructor(bridge: EditorViewBridge, options: EditorShellOptions = {}) {
        this.#bridge = bridge;
        this.#state = createEditorShellState(options);
    }

    state(): EditorShellState {
        return cloneEditorShellState(this.#state);
    }

    async syncView(): Promise<EditorShellState> {
        this.#applyViewDocument(await this.#bridge.snapshot());
        return this.state();
    }

    async applyViewportCommand(command: EditorViewportCommand): Promise<EditorViewBridgeCommandResult> {
        const result = await this.#bridge.applyViewportCommand(command);
        this.#applyViewDocument(result.document);
        return result;
    }

    async bindRenderPlane(
        viewportId: EditorViewportId,
        descriptor: EditorRenderPlaneDescriptor,
    ): Promise<EditorViewBridgeRenderPlaneResult> {
        const result = await this.#bridge.bindRenderPlane(viewportId, descriptor);
        this.#applyViewDocument(result.document);
        return result;
    }

    async presentRenderPlaneFrame(
        viewportId: EditorViewportId,
        frame: EditorRenderFramePresentation,
    ): Promise<EditorViewBridgeRenderPlaneResult> {
        const result = await this.#bridge.presentRenderPlaneFrame(viewportId, frame);
        this.#applyViewDocument(result.document);
        return result;
    }

    async releaseRenderPlane(viewportId: EditorViewportId): Promise<EditorViewBridgeRenderPlaneResult> {
        const result = await this.#bridge.releaseRenderPlane(viewportId);
        this.#applyViewDocument(result.document);
        return result;
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

    async close(): Promise<void> {
        await this.#bridge.close();
    }

    #applyViewDocument(document: EditorViewBridgeDocument): void {
        const clone = cloneEditorViewBridgeDocument(document);
        this.#state.protocol = clone.protocol;
        this.#state.activeViewportId = clone.activeViewportId;
        this.#state.viewports = clone.viewports;
    }
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
