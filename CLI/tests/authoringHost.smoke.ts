#!/usr/bin/env node

import assert from "node:assert/strict";

import { AuthoringHostClient } from "../client/authoringHostClient.ts";

const hostPath = process.argv[2];
assert.ok(hostPath, "Missing LunaAuthoringHost path.");

const client = new AuthoringHostClient(hostPath);
try {
    const capabilities = await client.capabilities();
    assert.ok(capabilities.capabilities.some((capability) => capability.op === "snapshot"));

    const initialSession = await client.session();
    assert.equal(initialSession.hasScene, true);
    assert.equal(initialSession.hasOpenTransaction, false);
    assert.equal(initialSession.canUndo, false);
    assert.equal(initialSession.canRedo, false);

    const beginRollback = await client.beginTransaction("AI Session Rollback");
    assert.equal(beginRollback.ok, true);
    assert.equal(beginRollback.session.hasOpenTransaction, true);

    const executeResult = await client.executePlan({
        commands: [
            { op: "new" },
            { op: "primitive", alias: "HostCube", mesh: "Cube" },
            { op: "snapshot" },
        ],
    });
    assert.equal(executeResult.ok, true);
    assert.equal(executeResult.report.scene.entityCount, 3);
    assert.ok(executeResult.events.length > 0);
    assert.equal((await client.session()).hasOpenTransaction, true);
    assert.equal((await client.session()).scene.entityCount, 3);

    const rollbackResult = await client.rollbackTransaction();
    assert.equal(rollbackResult.ok, true);
    assert.equal(rollbackResult.scene.entityCount, 0);
    assert.ok(rollbackResult.events.length > 0);

    const rolledBackSession = await client.session();
    assert.equal(rolledBackSession.hasOpenTransaction, false);
    assert.equal(rolledBackSession.canUndo, false);
    assert.equal(rolledBackSession.scene.entityCount, 0);

    const snapshotResult = await client.snapshot();
    assert.equal(snapshotResult.ok, true);
    assert.ok(snapshotResult.report.inspections.length > 0);

    const beginCommit = await client.beginTransaction("AI Session Commit");
    assert.equal(beginCommit.ok, true);

    const commitPlan = await client.executePlan({
        commands: [
            { op: "new" },
            { op: "primitive", alias: "CommittedCube", mesh: "Cube" },
            { op: "snapshot" },
        ],
    });
    assert.equal(commitPlan.ok, true);

    const commitResult = await client.commitTransaction();
    assert.equal(commitResult.ok, true);
    assert.equal(commitResult.scene.entityCount, 3);

    const committedSession = await client.session();
    assert.equal(committedSession.hasOpenTransaction, false);
    assert.equal(committedSession.canUndo, true);
    assert.equal(committedSession.undoDepth, 1);
    assert.equal(committedSession.redoDepth, 0);
    assert.equal(committedSession.scene.entityCount, 3);

    const undoResult = await client.undo();
    assert.equal(undoResult.ok, true);
    assert.equal(undoResult.scene.entityCount, 0);

    const redoResult = await client.redo();
    assert.equal(redoResult.ok, true);
    assert.equal(redoResult.scene.entityCount, 3);

    const clearHistoryResult = await client.clearHistory();
    assert.equal(clearHistoryResult.ok, true);
    assert.equal(clearHistoryResult.session.canUndo, false);
    assert.equal(clearHistoryResult.session.canRedo, false);
    assert.equal(clearHistoryResult.session.undoDepth, 0);
    assert.equal(clearHistoryResult.session.redoDepth, 0);

    assert.equal(await client.clearAliases(), true);
} finally {
    await client.close();
}
