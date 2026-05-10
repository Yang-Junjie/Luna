#!/usr/bin/env node

import assert from "node:assert/strict";
import { resolve } from "node:path";

import { EditorShellSession } from "../../Editor/Ts/shell/editorShellSession.ts";
import { EditorViewHostClient } from "../../Editor/Ts/bridge/editorViewHostClient.ts";
import {
    createEditorRenderPlaneDescriptor,
    createEditorViewportState,
} from "../../Editor/Ts/protocol/index.ts";

const [hostPath] = process.argv.slice(2);
assert.ok(hostPath, "Missing LunaEditorViewHost path.");

const bridge = new EditorViewHostClient(resolve(hostPath));
const session = new EditorShellSession(bridge);

try {
    const initial = await session.syncView();
    assert.equal(initial.protocol.name, "luna.editor");
    assert.equal(initial.protocol.version, 1);
    assert.equal(initial.activeViewportId, 1);
    assert.equal(Object.keys(initial.viewports).length, 1);
    assert.equal(initial.viewports["1"]?.viewportId, 1);

    const createCommand = {
        kind: "createViewport" as const,
        viewportId: 2,
        title: "Scene View",
        viewportKind: "scene" as const,
        size: { width: 1280, height: 720 },
        camera: createEditorViewportState(2, 7).camera,
        interaction: createEditorViewportState(2, 7).interaction,
        transformTool: "translate" as const,
        transformSpace: "local" as const,
        debugViewMode: "none" as const,
        debugVelocityScale: 20,
    };

    const createResult = await session.applyViewportCommand(createCommand);
    assert.equal(createResult.accepted, true);
    assert.equal(createResult.changed, true);
    assert.equal(session.state().activeViewportId, 2);
    assert.equal(session.state().viewports["2"]?.title, "Scene View");

    const descriptor = createEditorRenderPlaneDescriptor({
        planeId: 11,
        viewportId: 2,
        kind: "sceneViewport",
        transport: "sharedTexture",
        format: "RGBA8_UNORM",
        width: 1280,
        height: 720,
        presentable: true,
        bindingToken: "renderer.scene_output.texture",
        label: "Scene View",
    });

    const bindResult = await session.bindRenderPlane(2, descriptor);
    assert.equal(bindResult.ok, true);
    assert.equal(bindResult.changed, true);
    assert.equal(session.state().viewports["2"]?.renderPlane.descriptor.transport, "sharedTexture");

    const presentResult = await session.presentRenderPlaneFrame(2, {
        frameId: 5,
        sequence: 9,
        timestampNs: 123,
    });
    assert.equal(presentResult.ok, true);
    assert.equal(session.state().viewports["2"]?.renderPlane.frame.frameId, 5);

    const releaseResult = await session.releaseRenderPlane(2);
    assert.equal(releaseResult.ok, true);
    assert.equal(releaseResult.changed, true);

    const destroyResult = await session.applyViewportCommand({
        kind: "destroyViewport",
        viewportId: 2,
    });
    assert.equal(destroyResult.changed, true);
    assert.equal(session.state().activeViewportId, null);
    assert.equal(session.state().viewports["2"]?.viewportId, 0);
} finally {
    await session.close();
}
