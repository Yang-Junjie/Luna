import { EditorShellStore } from "../shell/editorShell.ts";
import {
    bindEditorRenderPlane,
    presentEditorRenderPlaneFrame,
    releaseEditorRenderPlane,
    type EditorRenderPlaneDescriptor,
} from "../protocol/renderDataPlane.ts";
import type { EditorViewportCommand } from "../protocol/viewProtocol.ts";
import type { EditorViewportId } from "../protocol/editorProtocol.ts";
import {
    editorShellStateToViewBridgeDocument,
    type EditorRenderFramePresentation,
    type EditorViewBridge,
    type EditorViewBridgeCommandResult,
    type EditorViewBridgeDocument,
    type EditorViewBridgeRenderPlaneResult,
} from "./editorViewBridge.ts";

export class LocalEditorViewBridge implements EditorViewBridge {
    #store: EditorShellStore;

    constructor(store = new EditorShellStore()) {
        this.#store = store;
    }

    async snapshot(): Promise<EditorViewBridgeDocument> {
        return editorShellStateToViewBridgeDocument(this.#store.state());
    }

    async applyViewportCommand(command: EditorViewportCommand): Promise<EditorViewBridgeCommandResult> {
        const result = this.#store.applyViewportCommand(command);
        return {
            ...result,
            document: await this.snapshot(),
        };
    }

    async bindRenderPlane(
        viewportId: EditorViewportId,
        descriptor: EditorRenderPlaneDescriptor,
    ): Promise<EditorViewBridgeRenderPlaneResult> {
        const viewport = this.#store.ensureViewport(viewportId, descriptor.planeId);
        const changed = bindEditorRenderPlane(viewport.renderPlane, descriptor);
        return {
            ok: true,
            changed,
            document: await this.snapshot(),
        };
    }

    async presentRenderPlaneFrame(
        viewportId: EditorViewportId,
        frame: EditorRenderFramePresentation,
    ): Promise<EditorViewBridgeRenderPlaneResult> {
        const viewport = this.#store.ensureViewport(viewportId);
        presentEditorRenderPlaneFrame(viewport.renderPlane, frame.frameId, frame.sequence, frame.timestampNs);
        return {
            ok: true,
            changed: true,
            document: await this.snapshot(),
        };
    }

    async releaseRenderPlane(viewportId: EditorViewportId): Promise<EditorViewBridgeRenderPlaneResult> {
        const viewport = this.#store.ensureViewport(viewportId);
        const changed = releaseEditorRenderPlane(viewport.renderPlane);
        return {
            ok: true,
            changed,
            document: await this.snapshot(),
        };
    }

    async close(): Promise<void> {}
}
