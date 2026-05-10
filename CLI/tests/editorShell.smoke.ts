#!/usr/bin/env node

import assert from "node:assert/strict";

import {
    applyEditorViewportCommand,
    bindEditorRenderPlane,
    createEditorRenderPlaneDescriptor,
    createEditorRenderPlaneState,
    createEditorViewportState,
    editorRenderTransportIsRealtime,
    releaseEditorRenderPlane,
    sameEditorViewportState,
    type EditorViewportCommand,
} from "../../Editor/Ts/index.ts";
import { EditorShellStore, createEditorShellState, listEditorShellViewports } from "../../Editor/Ts/index.ts";

const shell = new EditorShellStore();
const state = shell.state();

assert.equal(state.protocol.name, "luna.editor");
assert.equal(state.protocol.version, 1);
assert.equal(state.activeViewportId, 1);
assert.equal(listEditorShellViewports(state).length, 1);

const viewport = shell.ensureViewport(2, 7);
assert.equal(viewport.viewportId, 2);
assert.equal(viewport.renderPlane.descriptor.planeId, 7);

const command: EditorViewportCommand = {
    kind: "createViewport",
    viewportId: 2,
    title: "Scene View",
    viewportKind: "scene",
    size: { width: 1280, height: 720 },
    camera: createEditorViewportState(2, 7).camera,
    interaction: createEditorViewportState(2, 7).interaction,
    transformTool: "translate",
    transformSpace: "local",
    debugViewMode: "none",
    debugVelocityScale: 20,
};

const commandResult = shell.applyViewportCommand(command);
assert.equal(commandResult.accepted, true);
assert.equal(commandResult.changed, true);
assert.equal(shell.state().activeViewportId, 2);

const appliedViewport = shell.viewport(2);
assert.ok(appliedViewport);
assert.equal(appliedViewport?.title, "Scene View");
assert.equal(appliedViewport?.size.width, 1280);
assert.equal(appliedViewport?.size.height, 720);

const destroyResult = shell.applyViewportCommand({
    kind: "destroyViewport",
    viewportId: 2,
});
assert.equal(destroyResult.changed, true);
const destroyedViewport = shell.viewport(2);
assert.ok(destroyedViewport);
assert.equal(destroyedViewport?.viewportId, 0);
assert.equal(destroyedViewport?.title, "Viewport");
assert.equal(destroyedViewport?.renderPlane.descriptor.planeId, 0);
assert.equal(destroyedViewport?.renderPlane.descriptor.label, "");
assert.equal(shell.state().activeViewportId, null);

const manualViewport = createEditorViewportState(3, 9);
const resizeResult = applyEditorViewportCommand(manualViewport, {
    kind: "resizeViewport",
    viewportId: 3,
    size: { width: 800, height: 600 },
});
assert.equal(resizeResult.changed, true);
assert.equal(manualViewport.size.width, 800);
assert.equal(manualViewport.size.height, 600);

const planeState = createEditorRenderPlaneState();
const planeDescriptor = createEditorRenderPlaneDescriptor({
    planeId: 11,
    viewportId: 3,
    kind: "sceneViewport",
    transport: "sharedTexture",
    format: "RGBA8_UNORM",
    width: 800,
    height: 600,
    presentable: true,
    bindingToken: "renderer.scene_output.texture",
    label: "Scene View",
});

assert.equal(editorRenderTransportIsRealtime(planeDescriptor.transport), true);
assert.equal(bindEditorRenderPlane(planeState, planeDescriptor), true);
assert.equal(planeState.active, true);
assert.equal(planeState.frame.planeId, 11);
assert.equal(planeState.frame.ready, false);
assert.equal(planeState.descriptor.generation, 1);

planeState.frame.frameId = 5;
planeState.frame.sequence = 9;
planeState.frame.timestampNs = 123;
planeState.frame.width = 800;
planeState.frame.height = 600;
planeState.frame.ready = true;
assert.equal(releaseEditorRenderPlane(planeState), true);
assert.equal(planeState.active, false);
assert.equal(planeState.descriptor.viewportId, 3);

const clone = createEditorShellState({
    initialViewports: [createEditorViewportState(5, 5)],
    activeViewportId: 5,
});
assert.equal(clone.activeViewportId, 5);
assert.equal(listEditorShellViewports(clone).length, 1);
assert.equal(sameEditorViewportState(listEditorShellViewports(clone)[0], createEditorViewportState(5, 5)), true);
