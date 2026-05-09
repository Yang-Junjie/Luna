#!/usr/bin/env node

import assert from "node:assert/strict";

import { AuthoringHostClient } from "../client/authoringHostClient.ts";
import { AuthoringSessionController } from "../client/authoringController.ts";
import { AuthoringPlanBuilder } from "../client/authoringProtocol.ts";

const hostPath = process.argv[2];
assert.ok(hostPath, "Missing LunaAuthoringHost path.");

const client = new AuthoringHostClient(hostPath);
const controller = new AuthoringSessionController(client);

try {
    const state = await controller.state();
    assert.ok(state.capabilities.capabilities.some((capability) => capability.op === "snapshot"));
    assert.equal(state.session.hasOpenTransaction, false);

    const successPlan = new AuthoringPlanBuilder()
        .newScene()
        .primitive("ControllerCube", "Cube")
        .snapshot()
        .toPlan();

    const success = await controller.runPlan(successPlan);
    assert.equal(success.ok, true);
    assert.equal(success.openedTransaction, true);
    assert.equal(success.sessionBefore.hasOpenTransaction, false);
    assert.equal(success.sessionAfter.hasOpenTransaction, true);
    assert.equal(success.sessionAfter.scene.entityCount, 3);
    assert.equal(controller.lastReport()?.ok, true);
    assert.equal(controller.lastRepair()?.retryable, false);
    assert.ok(controller.lastEvents().length > 0);

    const commit = await controller.commitTransaction();
    assert.equal(commit.ok, true);
    assert.equal(commit.session.hasOpenTransaction, false);
    assert.equal(commit.session.canUndo, true);
    assert.equal(commit.session.undoDepth, 1);

    const failurePlan = new AuthoringPlanBuilder()
        .verifyEntityExists("MissingEntity")
        .toPlan();

    const failure = await controller.runPlan(failurePlan);
    assert.equal(failure.ok, false);
    assert.equal(failure.openedTransaction, true);
    assert.equal(failure.sessionBefore.hasOpenTransaction, false);
    assert.equal(failure.sessionAfter.hasOpenTransaction, true);
    assert.equal(failure.report.ok, false);
    assert.equal(controller.lastReport()?.ok, false);
    assert.equal(controller.lastRepair()?.retryable, true);

    const rollback = await controller.rollbackTransaction();
    assert.equal(rollback.ok, true);
    assert.equal(rollback.session.hasOpenTransaction, false);
    assert.equal(rollback.session.scene.entityCount, 3);

    const cleared = await controller.clearHistory();
    assert.equal(cleared.ok, true);
    assert.equal(cleared.session.canUndo, false);
    assert.equal(cleared.session.canRedo, false);
    assert.equal(cleared.session.undoDepth, 0);
    assert.equal(cleared.session.redoDepth, 0);
} finally {
    await client.close();
}
