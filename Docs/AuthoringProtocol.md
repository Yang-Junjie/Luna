# Luna Authoring Protocol

`luna.authoring` is the shared scene-authoring protocol used by CLI, future MCP tools, and editor bridges. The current protocol version is `1`.

## Execution Model

Commands are executed left-to-right against `AuthoringSession` through `AuthoringExecutor`.

`AuthoringExecutor` validates each plan before execution. Validation does not mutate the scene or write files; it checks protocol compatibility, command references, builtin asset names, component requirements, verification conditions that can be known ahead of time, and filesystem boundaries for `open` and `save`.

`AuthoringExecutor` runs each valid plan inside one authoring transaction. A successful plan commits as one undo step. A failed plan rolls back scene mutations before returning diagnostics.

Command effects are explicit in the protocol layer:

- Scene mutation commands include `new`, entity creation, parenting, renaming, transforms, lights, cameras, `open`, and `save`.
- Filesystem read commands currently include `open`.
- Filesystem write commands currently include `save`.
- Read-only commands include `inspect`, `verify`, and `summary`.

Filesystem writes must be registered with the executor's side-effect boundary before the write happens. Today `save` snapshots the target `.lunascene` file first: a failed plan removes a newly-created scene file or restores the original contents of an overwritten file. Future file-writing authoring commands, such as asset import, project save, or script generation, should declare `WritesFileSystem` and add a matching rollback boundary before implementation.

Dry-run mode uses the same validation path and exits before execution:

```powershell
build\CLI\LunaCLI.exe --dry-run --json new primitive Box Cube save Generated/BoxScene
```

Errors are terminal:

- Parse error: `ok=false`, no commands execute.
- Validation error: `ok=false`, no commands execute.
- Execution error: `ok=false`, execution stops at the failing command.
- Verification failure: `ok=false`, execution stops at the failing verification.
- Inspect commands do not mutate the scene.
- `summary` is text-only and has no JSON-side effect.

## History And Transactions

Editor actions, AI plans, and future MCP tools share the same `AuthoringSession` history layer.

- `beginTransaction(name)` captures the scene state before a batch.
- `commitTransaction()` captures the scene state after the batch and pushes one undo step.
- `rollbackTransaction()` restores the pre-transaction scene state.
- `undo()` and `redo()` restore committed transaction snapshots.

The current implementation uses scene-level snapshots for correctness and broad coverage. High-frequency editor interactions, such as viewport gizmo transforms, should use an explicit transaction so a drag gesture becomes one undo step.

## CLI Commands

Global options:

- `--json`
- `--dry-run`
- `--project <path>`

Authoring commands:

- `new`
- `open <scene-path>`
- `save <scene-path>`
- `entity <alias> <name>`
- `camera <alias>`
- `directional-light <alias>`
- `point-light <alias>`
- `spot-light <alias>`
- `primitive <alias> <Cube|Sphere|Plane|Cylinder|Cone>`
- `parent <child-ref> <parent-ref>`
- `unparent <child-ref>`
- `name <entity-ref> <name>`
- `transform <entity-ref> tx ty tz rxDeg ryDeg rzDeg sx sy sz`
- `light-intensity <entity-ref> <value>`
- `light-color <entity-ref> r g b`
- `camera-perspective <entity-ref> fovDeg near far`
- `camera-orthographic <entity-ref> size near far`

Query commands:

- `inspect scene`
- `inspect entity <entity-ref>`
- `inspect hierarchy`
- `snapshot`

Verification commands:

- `verify saved`
- `verify entity <entity-ref>`
- `verify component <entity-ref> <component>`
- `verify entity-count-at-least <count>`

Entity refs are aliases created earlier in the same execution or numeric UUIDs.

## Plan JSON

Plan files are JSON objects:

```json
{
  "protocol": { "name": "luna.authoring", "version": 1 },
  "project": "optional/path/to/project.lunaproj",
  "commands": [
    { "op": "new" },
    { "op": "primitive", "alias": "Box", "mesh": "Cube" },
    { "op": "inspect", "target": "entity", "entity": "Box" },
    { "op": "verify", "check": "hasComponent", "entity": "Box", "component": "Mesh" },
    { "op": "snapshot" }
  ]
}
```

The `protocol` field is optional for v1 compatibility. If present, `name` must be `luna.authoring` and `version` must be `1`.

Plan JSON uses stable protocol names rather than the CLI shorthand. For example, the scene-saved verification is `{ "op": "verify", "check": "sceneSaved" }`. The JSON loader still accepts the legacy check name `"saved"` for compatibility, but writers emit `"sceneSaved"`.

`snapshot` is the stable read endpoint for AI and editor refreshes. It appends a hierarchy inspection to the report and leaves scene mutation state unchanged.

Machine-readable JSON Schema files are kept in `Docs/Schemas`:

- `authoring-plan.schema.json`
- `authoring-report.schema.json`
- `authoring-capabilities.schema.json`

Golden plan fixtures are kept in `Tests/Fixtures/Authoring` and are covered by `AuthoringProtocolTests`. These fixtures are the compatibility samples for CLI, editor, AI, and future MCP integrations.

The TypeScript CLI client keeps its typed protocol facade in `CLI/client/authoringProtocol.ts`. Future AI-facing client code should build plans through that facade instead of hand-writing command objects in multiple places. The same facade normalizes report JSON into typed scene snapshots, entity bindings, inspections, verifications, and diagnostics so TS editor panels and AI agents can consume `snapshot` results without treating report arrays as unstructured data.

Use the report helper APIs for common scene reads:

- `latestHierarchy(report)`
- `latestHierarchyEntities(report)`
- `findEntityBinding(report, ref)`
- `findEntityByRef(report, ref)`
- `findEntityByUuid(report, uuid)`
- `findEntityByName(report, name)`
- `findEntitiesByComponent(report, component)`

The interactive TS client also queues typed `PlanCommand` objects and executes them by writing a temporary plan JSON for the C++ `plan` entrypoint.

## Capabilities

The authoring capability registry describes what this engine build can currently author. It is a discovery layer for AI, editor command palettes, plugins, and future MCP tools; it does not execute commands.

Export the current capability list:

```powershell
build\CLI\LunaCLI.exe capabilities --json
```

Capability JSON includes:

- `op`: stable plan operation name
- `title` and `description`: concise human/model-facing description
- `effects`: declared side effects such as `readsScene`, `mutatesScene`, `readsFileSystem`, and `writesFileSystem`
- `requiresConfirmation`: whether a UI or agent should ask before running the command
- `parameters`: machine-readable parameter descriptions and enum hints
- `examples`: valid command JSON examples

AI clients should use capabilities as the first layer of runtime context, then use the plan schema and diagnostics for validation and repair.

The TS client has a local compose skeleton that exercises this flow without a model provider:

```powershell
node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts capabilities
node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts compose "create a simple scene with a cube, camera, and light"
node --disable-warning=ExperimentalWarning --experimental-strip-types CLI/client/luna.ts --execute compose "create a simple scene with a cube, floor, camera, and light"
```

By default `compose` prints a structured result containing the generated plan, the dry-run report, and a compact repair context. `--execute` runs the plan and returns the same result shape with `mode` set to `execute`. `--plan-only` prints only the generated plan. The current composer is intentionally rule-based; future OpenAI-style integration should replace only the intent-to-plan step while keeping the same capabilities, plan, dry-run, report, and diagnostics loop.

The compose result shape is:

```json
{
  "ok": true,
  "mode": "dry-run",
  "intent": "create a simple scene with a cube",
  "plan": {},
  "report": {},
  "repair": {
    "ok": true,
    "diagnostics": [],
    "errors": [],
    "retryable": false
  }
}
```

## Report JSON

`LunaCLI --json ...` emits one report object:

```json
{
  "protocol": { "name": "luna.authoring", "version": 1 },
  "ok": true,
  "scene": {
    "name": "SceneName",
    "path": "SceneName.lunascene",
    "entityCount": 3,
    "dirty": false
  },
  "entities": [],
  "savedScenes": [],
  "inspections": [],
  "verifications": [],
  "diagnostics": [],
  "errors": []
}
```

Exit code is `0` when `ok=true`, otherwise `1`.

`errors` is retained as the legacy human-readable error list. New automation, AI, and editor integrations should prefer `diagnostics`.

Diagnostics may also carry machine-readable hint fields:

- `recoverable`
- `expected`
- `actual`
- `suggestedCommand`

Diagnostics are structured objects:

```json
{
  "severity": "error",
  "phase": "execute",
  "code": "UnknownEntity",
  "message": "Unknown entity reference 'Box'.",
  "commandIndex": 2,
  "command": "transform",
  "field": null,
  "entityRef": "Box",
  "component": null,
  "path": null,
  "recoverable": true,
  "expected": "resolvable entity reference",
  "actual": "Box",
  "suggestedCommand": null
}
```

Stable diagnostic codes currently include:

- `InvalidPlan`
- `ProtocolMismatch`
- `UnsupportedCommand`
- `UnsupportedVerifyCheck`
- `MissingArgument`
- `InvalidArgument`
- `InvalidNumber`
- `NoBoundScene`
- `UnknownEntity`
- `UnknownBuiltinAsset`
- `MissingComponent`
- `OpenSceneFailed`
- `SaveSceneFailed`
- `FileOverwrite`
- `ProjectLoadFailed`
- `ExecutionFailed`
- `VerificationFailed`

## Versioning

Protocol version `1` is additive by default. New commands, optional fields, inspections, and verification kinds may be added without changing the version. Renaming fields, changing command semantics, or changing error/exit behavior requires a version bump.
