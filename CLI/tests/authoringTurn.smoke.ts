#!/usr/bin/env node

import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";

import { AuthoringHostClient } from "../client/authoringHostClient.ts";
import { AuthoringSessionController } from "../client/authoringController.ts";
import {
    AuthoringTurn,
    buildAuthoringTurnTransactionName,
    type AuthoringTurnPlanner,
} from "../client/authoringTurn.ts";
import { AuthoringPlanBuilder, findEntityByRef, latestHierarchy } from "../client/authoringProtocol.ts";

const [hostPath, clientPath] = process.argv.slice(2);
assert.ok(hostPath, "Missing LunaAuthoringHost path.");

const client = new AuthoringHostClient(hostPath);
try {
    const controller = new AuthoringSessionController(client);
    const turn = new AuthoringTurn(controller);
    let plannerCalls = 0;
    const planner: AuthoringTurnPlanner = ({ attempt, previousAttempt }) => {
        ++plannerCalls;
        if (attempt === 1) {
            assert.equal(previousAttempt, undefined);
            return new AuthoringPlanBuilder()
                .verifyEntityExists("MissingEntity")
                .toPlan();
        }

        assert.equal(previousAttempt?.ok, false);
        assert.equal(previousAttempt?.repair.retryable, true);
        return new AuthoringPlanBuilder()
            .newScene()
            .primitive("TurnCube", "Cube")
            .inspectHierarchy()
            .snapshot()
            .toPlan();
    };

    const result = await turn.run("retry into a cube scene", planner, {
        transactionName: buildAuthoringTurnTransactionName("retry into a cube scene"),
        maxAttempts: 2,
        commitOnSuccess: true,
    });

    assert.equal(plannerCalls, 2);
    assert.equal(result.ok, true);
    assert.equal(result.committed, true);
    assert.equal(result.attempts.length, 2);
    assert.equal(result.attempts[0]?.ok, false);
    assert.equal(result.attempts[0]?.committed, false);
    assert.equal(result.attempts[0]?.sessionAfter.hasOpenTransaction, false);
    assert.equal(result.attempts[1]?.ok, true);
    assert.equal(result.attempts[1]?.committed, true);
    assert.equal(result.sessionAfter.hasOpenTransaction, false);
    assert.equal(result.sessionAfter.canUndo, true);
    assert.ok(latestHierarchy(result.report));
    assert.equal(findEntityByRef(result.report, "TurnCube")?.components.includes("Mesh"), true);

    const dryRun = await turn.run(
        "dry run without commit",
        () => new AuthoringPlanBuilder()
            .newScene()
            .primitive("DryRunCube", "Cube")
            .snapshot()
            .toPlan(),
        {
            transactionName: buildAuthoringTurnTransactionName("dry run without commit"),
            maxAttempts: 1,
            commitOnSuccess: false,
        },
    );

    assert.equal(dryRun.ok, true);
    assert.equal(dryRun.committed, false);
    assert.equal(dryRun.sessionAfter.hasOpenTransaction, false);
    assert.equal(dryRun.sessionAfter.scene.entityCount, result.sessionAfter.scene.entityCount);
    assert.equal((await controller.session()).hasOpenTransaction, false);
} finally {
    await client.close();
}

if (clientPath != null) {
    const cli = spawnSync(
        process.execPath,
        [
            "--disable-warning=ExperimentalWarning",
            "--experimental-strip-types",
            clientPath,
            "--authoring-host",
            hostPath,
            "turn",
            "create a simple scene with a cube",
            "--execute",
        ],
        { encoding: "utf8" },
    );

    assert.equal(cli.status, 0, cli.stderr);
    const cliResult = JSON.parse(cli.stdout) as {
        ok: boolean;
        committed: boolean;
        mode: string;
        attempts: unknown[];
        plan: { commands: Array<Record<string, unknown>> };
    };
    assert.equal(cliResult.ok, true);
    assert.equal(cliResult.committed, true);
    assert.equal(cliResult.mode, "execute");
    assert.equal(cliResult.attempts.length, 1);
    assert.ok(cliResult.plan.commands.some((command) => command.op === "primitive" && command.alias === "Cube"));
}
